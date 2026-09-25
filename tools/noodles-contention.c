/* Measures whether Noodles engine traffic slows ARM memory access through
   the shared DDR3 controller.

   A measuring thread pinned to CPU 0 times dependent-load latency over a
   buffer much larger than the caches, streaming read bandwidth and copy
   bandwidth. Each phase repeats those measurements while a load thread
   pinned to CPU 1 keeps the engine idle or busy with full-surface fills,
   plain draws, standard-alpha blends or a mix of the three between
   off-screen surfaces, and reports the engine rate it achieved. Run it
   alone while the core is loaded: GemRB must not be running. */

#define _GNU_SOURCE
#include <noodles_link.h>
#include <noodles_surface.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W NOODLES_BUFFER_WIDTH
#define H NOODLES_BUFFER_HEIGHT
#define LINE 64u
#define CHASE_BYTES (64u << 20)
#define STREAM_BYTES (16u << 20)
#define MEASURE_MS 1000.0
#define WARMUP_MS 200.0

static noodles_link_t *link;
static noodles_surface_t *dst, *opaque, *half;

typedef enum { LOAD_NONE, LOAD_FILL, LOAD_PLAIN, LOAD_BLEND, LOAD_MIX } load_kind;
static const char *const load_names[] = {"idle", "fill", "plain draw", "blend", "mix"};

static volatile int stop_load;
static load_kind current_load;
static uint64_t load_pixels;
static double load_ms;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}

static void fail(const char *operation)
{
    perror(operation);
    exit(EXIT_FAILURE);
}

static void pin(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) fail("sched_setaffinity");
}

/* Retries a nonblocking submission while the ring or a descriptor table is
   full, waiting for one command of progress at a time. */
#define SUBMIT(what, call) do {                                                   \
        while ((call) != 0) {                                                     \
            if (errno != EAGAIN) fail(what);                                      \
            if (noodles_link_wait_progress(link, NOODLES_DEFAULT_TIMEOUT_MS)) fail(what); \
        }                                                                         \
    } while (0)

static void submit_fill(void)
{
    const noodles_rect_t all = {0, 0, W, H};
    SUBMIT("fill", noodles_surface_fill(dst, &all, 0xff203040u));
}

static void submit_draw(const noodles_surface_t *source, uint32_t flags)
{
    const noodles_rect_t all = {0, 0, W, H};
    const noodles_surface_draw_t draw = {source, all, 0, 0, flags, 0xffffffffu};
    SUBMIT("draw", noodles_surface_draw_batch(link, dst, &draw, 1));
}

static void *load_thread(void *unused)
{
    const double start = now_ms();
    uint64_t pixels = 0;
    unsigned step = 0;
    (void)unused;
    pin(1);
    while (current_load != LOAD_NONE && !stop_load) {
        load_kind kind = current_load;
        if (kind == LOAD_MIX) kind = (load_kind)(LOAD_FILL + step++ % 3);
        if (kind == LOAD_FILL) submit_fill();
        else submit_draw(kind == LOAD_PLAIN ? opaque : half,
                         kind == LOAD_PLAIN ? 0 : NOODLES_DRAW_BLEND);
        pixels += (uint64_t)W * H;
    }
    if (noodles_link_drain(link, NOODLES_DEFAULT_TIMEOUT_MS) != 0) fail("drain");
    load_pixels = pixels;
    load_ms = now_ms() - start;
    return NULL;
}

/* A random cyclic permutation of cache lines, so every load depends on the
   previous one and hardware prefetch cannot hide DDR3 latency. */
static void **build_chase(void)
{
    const size_t lines = CHASE_BYTES / LINE;
    char *base = aligned_alloc(LINE, CHASE_BYTES);
    size_t *order = malloc(lines * sizeof(*order));
    size_t i;
    uint32_t seed = 0x2545f491u;
    if (!base || !order) fail("allocation");
    for (i = 0; i < lines; ++i) order[i] = i;
    for (i = lines - 1; i > 0; --i) {  /* Sattolo's algorithm: one cycle */
        size_t j;
        seed = seed * 1664525u + 1013904223u;
        j = seed % i;
        size_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    for (i = 0; i < lines; ++i)
        *(void **)(base + order[i] * LINE) = base + order[(i + 1) % lines] * LINE;
    free(order);
    return (void **)base;
}

static double measure_latency(void **chase)
{
    void **p = chase;
    uint64_t loads = 0;
    const double start = now_ms();
    double elapsed;
    do {
        for (int i = 0; i < 4096; ++i) p = (void **)*p;
        loads += 4096;
        elapsed = now_ms() - start;
    } while (elapsed < MEASURE_MS);
    if (!p) fail("chase");  /* keep the chain live */
    return elapsed * 1e6 / (double)loads;
}

static double measure_read(const uint64_t *buffer)
{
    const size_t words = STREAM_BYTES / sizeof(*buffer);
    uint64_t sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0, bytes = 0;
    volatile uint64_t sink;
    const double start = now_ms();
    double elapsed;
    do {
        for (size_t i = 0; i < words; i += 4) {
            sum0 += buffer[i]; sum1 += buffer[i + 1];
            sum2 += buffer[i + 2]; sum3 += buffer[i + 3];
        }
        bytes += STREAM_BYTES;
        elapsed = now_ms() - start;
    } while (elapsed < MEASURE_MS);
    sink = sum0 + sum1 + sum2 + sum3;
    (void)sink;
    return (double)bytes / elapsed * 1e-3;
}

static double measure_copy(void *to, const void *from)
{
    uint64_t bytes = 0;
    const double start = now_ms();
    double elapsed;
    do {
        memcpy(to, from, STREAM_BYTES);
        bytes += STREAM_BYTES;
        elapsed = now_ms() - start;
    } while (elapsed < MEASURE_MS);
    return (double)bytes / elapsed * 1e-3;
}

typedef struct { double engine, latency, read, copy; } result;

static result run_phase(load_kind kind, void **chase, uint64_t *stream, void *copy_to)
{
    pthread_t thread;
    result r;
    double start;
    current_load = kind;
    stop_load = 0;
    load_pixels = 0;
    load_ms = 0.0;
    if (kind != LOAD_NONE && pthread_create(&thread, NULL, load_thread, NULL) != 0)
        fail("pthread_create");
    start = now_ms();
    while (now_ms() - start < WARMUP_MS) {
    }
    r.latency = measure_latency(chase);
    r.read = measure_read(stream);
    r.copy = measure_copy(copy_to, stream);
    if (kind != LOAD_NONE) {
        stop_load = 1;
        pthread_join(thread, NULL);
    }
    r.engine = load_ms > 0.0 ? (double)load_pixels / load_ms * 1e-3 : 0.0;
    return r;
}

int main(void)
{
    const noodles_rect_t all = {0, 0, W, H};
    const load_kind phases[] = {LOAD_NONE, LOAD_FILL, LOAD_PLAIN, LOAD_BLEND, LOAD_MIX,
                                LOAD_NONE};
    void **chase;
    uint64_t *stream;
    void *copy_to;
    result idle = {0, 0, 0, 0};
    size_t i;

    pin(0);
    if (noodles_link_open(&link) != 0) fail("noodles_link_open (is the core loaded and GemRB stopped?)");
    if (noodles_surface_create(link, W, H, &dst) != 0 ||
        noodles_surface_create(link, W, H, &opaque) != 0 ||
        noodles_surface_create(link, W, H, &half) != 0)
        fail("surface create");
    SUBMIT("setup", noodles_surface_fill(dst, &all, 0xff000000u));
    SUBMIT("setup", noodles_surface_fill(opaque, &all, 0xff4080c0u));
    SUBMIT("setup", noodles_surface_fill(half, &all, 0x804080c0u));
    if (noodles_link_drain(link, NOODLES_DEFAULT_TIMEOUT_MS) != 0) fail("setup");

    chase = build_chase();
    stream = aligned_alloc(LINE, STREAM_BYTES);
    copy_to = aligned_alloc(LINE, STREAM_BYTES);
    if (!stream || !copy_to) fail("allocation");
    memset(stream, 0x5a, STREAM_BYTES);
    memset(copy_to, 0, STREAM_BYTES);

    printf("ARM memory on CPU 0 while CPU 1 drives the engine (latency: %u MiB dependent chain; "
           "bandwidth: %u MiB buffers)\n", CHASE_BYTES >> 20, STREAM_BYTES >> 20);
    printf("%-11s %9s %11s %7s %10s %7s %10s %7s\n", "engine load", "Mpix/s", "latency ns",
           "x idle", "read MB/s", "% idle", "copy MB/s", "% idle");
    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const result r = run_phase(phases[i], chase, stream, copy_to);
        if (i == 0) idle = r;
        printf("%-11s %9.1f %11.1f %7.2f %10.0f %7.1f %10.0f %7.1f\n", load_names[phases[i]],
               r.engine, r.latency, r.latency / idle.latency, r.read, r.read * 100.0 / idle.read,
               r.copy, r.copy * 100.0 / idle.copy);
    }

    noodles_surface_destroy(half);
    noodles_surface_destroy(opaque);
    noodles_surface_destroy(dst);
    if (noodles_link_close(link, NOODLES_DEFAULT_TIMEOUT_MS) != 0) fail("close");
    return 0;
}
