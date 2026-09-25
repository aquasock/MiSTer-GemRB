/* Measures the loaded Noodles core's engine throughput and PRESENT pacing
   through the SDK, independently of GemRB and SDL.

   Rate tests repeat full-surface operations between managed off-screen
   surfaces and time them from first submission to drain. Pacing tests show
   how long PRESENT keeps the command queue busy and how that changes with
   the amount of draw work before it, with two display buffers and, on
   protocol 1.7 cores, again with three. They time the raw completion count the
   core publishes, separately from the SDK's confirmed wait, which also needs
   the core to answer a verification request. Run it alone while the core is
   loaded: it is a separate SDK client and GemRB must not be running. */

#define _POSIX_C_SOURCE 199309L
#include <noodles_link.h>
#include <noodles_surface.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W NOODLES_BUFFER_WIDTH
#define H NOODLES_BUFFER_HEIGHT
#define ENGINE_HZ 100e6
#define SMALL 64u
#define PACING_FRAMES 40
#define PACING_WARMUP 4

static noodles_link_t *link;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}

static void fail(const char *operation)
{
    perror(operation);
    if (link) noodles_link_close(link, NOODLES_DEFAULT_TIMEOUT_MS);
    exit(EXIT_FAILURE);
}

/* Retries a nonblocking submission while the ring or a descriptor table is
   full, waiting for one command of progress at a time. */
#define SUBMIT(what, call) do {                                                   \
        while ((call) != 0) {                                                     \
            if (errno != EAGAIN) fail(what);                                      \
            if (noodles_link_wait_progress(link, NOODLES_DEFAULT_TIMEOUT_MS)) fail(what); \
        }                                                                         \
    } while (0)

/* Returns when the core's published completion count reaches target,
   without the SDK's verification round trip. */
static double raw_wait(noodles_fence_t target, const char *what)
{
    const struct timespec step = {0, 20000};
    const double deadline = now_ms() + NOODLES_DEFAULT_TIMEOUT_MS;
    for (;;) {
        const double now = now_ms();
        if (((noodles_link_done_count(link) - target) & 0x7fffffffu) < 0x40000000u) return now;
        if (now > deadline) {
            errno = ETIMEDOUT;
            fail(what);
        }
        nanosleep(&step, NULL);
    }
}

static double confirmed_wait(noodles_fence_t target, const char *what)
{
    if (noodles_link_wait(link, target, NOODLES_DEFAULT_TIMEOUT_MS) != 0) fail(what);
    return now_ms();
}

static void drain(const char *what)
{
    if (noodles_link_drain(link, NOODLES_DEFAULT_TIMEOUT_MS) != 0) fail(what);
}

static noodles_surface_t *surface(uint32_t color)
{
    noodles_surface_t *s;
    const noodles_rect_t all = {0, 0, W, H};
    if (noodles_surface_create(link, W, H, &s) != 0) fail("surface create");
    SUBMIT("surface fill", noodles_surface_fill(s, &all, color));
    return s;
}

typedef enum {
    OP_FILL, OP_FILL_BATCH, OP_BLEND_FILL, OP_BLIT, OP_DRAW, OP_SMALL_DRAWS
} op_kind;

typedef struct {
    const char *name;
    op_kind kind;
    const noodles_surface_t *source;
    uint32_t flags;
    uint32_t modulation;
    unsigned bytes_per_pixel;   /* nominal read + write bytes the operation implies */
} rate_test;

static void submit_op(const rate_test *t, noodles_surface_t *dst)
{
    const noodles_rect_t all = {0, 0, W, H};
    noodles_surface_fill_t fills[60];
    noodles_surface_draw_t draws[64];
    unsigned i;

    switch (t->kind) {
    case OP_FILL:
        SUBMIT("fill", noodles_surface_fill(dst, &all, 0xff203040u));
        break;
    case OP_FILL_BATCH:
        for (i = 0; i < 60; ++i) {
            fills[i].rect = (noodles_rect_t){0, (int32_t)(i * 10), W, 10};
            fills[i].color = 0xff000000u | i * 0x040404u;
        }
        SUBMIT("fill batch", noodles_surface_fill_batch(link, dst, fills, 60));
        break;
    case OP_BLEND_FILL:
        SUBMIT("blend fill", noodles_surface_blend_fill(dst, &all, 0x80406080u,
                                                        NOODLES_DRAW_MODE_BLEND));
        break;
    case OP_BLIT:
        SUBMIT("blit", noodles_surface_blit(dst, 0, 0, t->source, &all));
        break;
    case OP_DRAW:
        draws[0] = (noodles_surface_draw_t){t->source, all, 0, 0, t->flags, t->modulation};
        SUBMIT("draw", noodles_surface_draw_batch(link, dst, draws, 1));
        break;
    case OP_SMALL_DRAWS:
        for (i = 0; i < 64; ++i) {
            draws[i] = (noodles_surface_draw_t){
                t->source, {0, 0, SMALL, SMALL},
                (int32_t)(i % 12 * SMALL), (int32_t)(i / 12 * SMALL),
                t->flags, t->modulation};
        }
        SUBMIT("small draws", noodles_surface_draw_batch(link, dst, draws, 64));
        break;
    }
}

static uint64_t op_pixels(const rate_test *t)
{
    return t->kind == OP_SMALL_DRAWS ? 64u * SMALL * SMALL : (uint64_t)W * H;
}

static void run_rate(const rate_test *t, noodles_surface_t *dst, int reps)
{
    double start, ms;
    double pixels;
    int i;

    submit_op(t, dst);          /* warm the path and settle the queue */
    drain("rate warmup");
    start = now_ms();
    for (i = 0; i < reps; ++i) submit_op(t, dst);
    drain("rate");
    ms = (now_ms() - start) / reps;
    pixels = (double)op_pixels(t);
    printf("%-30s %3d %8.3f %8.1f %7.2f %6.2f %7.0f\n", t->name, reps, ms,
           pixels / ms * 1e-3, ms * 1e6 / pixels, ms * 1e-3 * ENGINE_HZ / pixels,
           pixels * t->bytes_per_pixel / ms * 1e-3);
}

static int compare(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

typedef struct { double mean, min, p50, max; } summary;

static summary summarize(double *v, int n)
{
    summary s = {0, 0, 0, 0};
    int i;
    for (i = 0; i < n; ++i) s.mean += v[i];
    s.mean /= n;
    qsort(v, (size_t)n, sizeof(*v), compare);
    s.min = v[0];
    s.p50 = v[n / 2];
    s.max = v[n - 1];
    return s;
}

/* Frame loop: k full-surface fills off screen, then PRESENT. Draw time runs
   from the start of the frame (the queue is idle after the previous PRESENT)
   to the fills' raw completion, PRESENT from there to its raw completion and
   confirmation from there to the SDK's confirmed wait. */
static void run_pacing(noodles_surface_t *dst, int k)
{
    const noodles_rect_t all = {0, 0, W, H};
    double draw[PACING_FRAMES], present[PACING_FRAMES], confirm[PACING_FRAMES];
    double period[PACING_FRAMES];
    double previous = 0.0;
    int frame, i;

    for (frame = -PACING_WARMUP; frame < PACING_FRAMES; ++frame) {
        noodles_fence_t draw_fence, present_fence;
        double start = now_ms(), drawn, presented, confirmed;
        for (i = 0; i < k; ++i) {
            SUBMIT("pacing fill", noodles_surface_fill(dst, &all, 0xff000000u | (uint32_t)i));
        }
        draw_fence = noodles_link_last_fence(link);
        SUBMIT("present", noodles_push_present(link, &present_fence));
        drawn = raw_wait(draw_fence, "draw wait");
        presented = raw_wait(present_fence, "present wait");
        confirmed = confirmed_wait(present_fence, "present confirmation");
        if (frame >= 0) {
            draw[frame] = drawn - start;
            present[frame] = presented - drawn;
            confirm[frame] = confirmed - presented;
            period[frame] = confirmed - previous;
        }
        previous = confirmed;
    }
    {
        const summary d = summarize(draw, PACING_FRAMES);
        const summary p = summarize(present, PACING_FRAMES);
        const summary c = summarize(confirm, PACING_FRAMES);
        const summary f = summarize(period, PACING_FRAMES);
        printf("%2d %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %6.2f\n", k,
               d.mean, p.mean, p.min, p.max, c.mean, f.mean, f.min, f.p50, f.max,
               1000.0 / f.mean);
    }
}

/* A fill pushed immediately behind PRESENT: its raw completion relative to
   the PRESENT's shows whether later commands can execute while the flip
   waits for the display, and the confirmed PRESENT wait shows whether the
   queued fill delays the SDK's verification of the earlier fence. A
   two-buffer PRESENT completes at the flip; a queued one on acceptance. */
static void run_behind_present(noodles_surface_t *dst)
{
    const noodles_rect_t all = {0, 0, W, H};
    double alone[PACING_FRAMES], flip[PACING_FRAMES], behind[PACING_FRAMES];
    double confirm[PACING_FRAMES];
    int frame;

    for (frame = -PACING_WARMUP; frame < PACING_FRAMES; ++frame) {
        noodles_fence_t present_fence, fill_fence;
        double start, filled, presented, confirmed;

        start = now_ms();
        SUBMIT("idle fill", noodles_surface_fill(dst, &all, 0xff102030u));
        drain("idle fill");
        if (frame >= 0) alone[frame] = now_ms() - start;

        start = now_ms();
        SUBMIT("present", noodles_push_present(link, &present_fence));
        SUBMIT("fill behind present", noodles_surface_fill(dst, &all, 0xff302010u));
        fill_fence = noodles_link_last_fence(link);
        presented = raw_wait(present_fence, "present wait");
        confirmed = confirmed_wait(present_fence, "present confirmation");
        filled = raw_wait(fill_fence, "fill wait");
        drain("behind present");
        if (frame >= 0) {
            flip[frame] = presented - start;
            behind[frame] = filled - presented;
            confirm[frame] = confirmed - presented;
        }
    }
    {
        const summary a = summarize(alone, PACING_FRAMES);
        const summary p = summarize(flip, PACING_FRAMES);
        const summary b = summarize(behind, PACING_FRAMES);
        const summary c = summarize(confirm, PACING_FRAMES);
        printf("fill alone %.2f ms; PRESENT raw %.2f ms (%.2f-%.2f); fill queued behind it "
               "completes %.2f ms after the PRESENT (%.2f-%.2f); PRESENT confirmation %.2f ms "
               "after its raw completion (%.2f-%.2f)\n",
               a.mean, p.mean, p.min, p.max, b.mean, b.min, b.max, c.mean, c.min, c.max);
    }
}

/* One fill followed by more queued fills: the delay between the first
   fill's raw completion and the SDK's confirmed wait on it, as the queued
   work behind it grows. */
static void run_confirmation(noodles_surface_t *dst, int queued)
{
    const noodles_rect_t all = {0, 0, W, H};
    double raw[PACING_FRAMES], confirm[PACING_FRAMES];
    int frame, i;

    for (frame = -PACING_WARMUP; frame < PACING_FRAMES; ++frame) {
        noodles_fence_t first;
        double start, reached, confirmed;
        drain("confirmation setup");
        start = now_ms();
        SUBMIT("first fill", noodles_surface_fill(dst, &all, 0xff405060u));
        first = noodles_link_last_fence(link);
        for (i = 0; i < queued; ++i) {
            SUBMIT("queued fill", noodles_surface_fill(dst, &all, 0xff000000u | (uint32_t)i));
        }
        reached = raw_wait(first, "first fill");
        confirmed = confirmed_wait(first, "first fill confirmation");
        if (frame >= 0) {
            raw[frame] = reached - start;
            confirm[frame] = confirmed - reached;
        }
    }
    drain("confirmation");
    {
        const summary r = summarize(raw, PACING_FRAMES);
        const summary c = summarize(confirm, PACING_FRAMES);
        printf("%2d %7.2f %7.2f %7.2f %7.2f\n", queued, r.mean, c.mean, c.min, c.max);
    }
}

static void run_presentation(noodles_surface_t *dst, int max_k)
{
    int k;
    printf("\nPRESENT pacing with %d display buffers: k full-surface off-screen fills per frame, "
           "%d frames each (ms)\n", noodles_link_buffer_count(link), PACING_FRAMES);
    printf("%2s %7s %7s %7s %7s %7s %7s %7s %7s %7s %6s\n", "k", "draw", "present", "min",
           "max", "confirm", "period", "min", "median", "max", "fps");
    for (k = 0; k <= max_k; ++k) run_pacing(dst, k);

    printf("\nQueue behind PRESENT with %d display buffers\n", noodles_link_buffer_count(link));
    run_behind_present(dst);
}

int main(int argc, char **argv)
{
    noodles_device_info_t info;
    noodles_surface_t *dst, *opaque, *clear, *half;
    const noodles_rect_t all = {0, 0, W, H};
    const int reps = argc > 1 ? atoi(argv[1]) : 20;
    int k;

    if (reps < 1 || reps > 1000) {
        fprintf(stderr, "usage: %s [repetitions 1-1000]\n", argv[0]);
        return 2;
    }
    if (noodles_link_open(&link) != 0) fail("noodles_link_open (is the core loaded and GemRB stopped?)");
    if (noodles_link_get_info(link, &info) != 0) fail("noodles_link_get_info");
    printf("Noodles protocol %u.%u, SDK %s, %ux%u\n", info.protocol_version >> 16,
           info.protocol_version & 0xffffu, info.sdk_version, info.width, info.height);

    dst = surface(0xff000000u);
    opaque = surface(0xff4080c0u);
    clear = surface(0x004080c0u);
    half = surface(0x804080c0u);
    SUBMIT("back buffer clear", noodles_back_buffer_fill(link, &all, 0xff000000u));
    if (noodles_present_and_wait(link) != 0) fail("present");
    SUBMIT("back buffer clear", noodles_back_buffer_fill(link, &all, 0xff000000u));
    if (noodles_present_and_wait(link) != 0) fail("present");
    drain("setup");

    {
        const rate_test tests[] = {
            {"solid fill 800x600", OP_FILL, NULL, 0, 0, 4},
            {"fill batch 60x800x10", OP_FILL_BATCH, NULL, 0, 0, 4},
            {"blend fill 800x600", OP_BLEND_FILL, NULL, 0, 0, 8},
            {"blit copy 800x600", OP_BLIT, opaque, 0, 0, 8},
            {"draw plain 800x600", OP_DRAW, opaque, 0, 0xffffffffu, 8},
            {"draw blend opaque src", OP_DRAW, opaque, NOODLES_DRAW_BLEND, 0xffffffffu, 12},
            {"draw blend clear src", OP_DRAW, clear, NOODLES_DRAW_BLEND, 0xffffffffu, 12},
            {"draw blend half src", OP_DRAW, half, NOODLES_DRAW_BLEND, 0xffffffffu, 12},
            {"draw blend half mirror-x", OP_DRAW, half, NOODLES_DRAW_BLEND | NOODLES_DRAW_MIRROR_X,
             0xffffffffu, 12},
            {"draw blend opaque alpha-mod", OP_DRAW, opaque, NOODLES_DRAW_BLEND, 0x80ffffffu, 12},
            {"64 draws 64x64 plain", OP_SMALL_DRAWS, opaque, 0, 0xffffffffu, 8},
            {"64 draws 64x64 blend half", OP_SMALL_DRAWS, half, NOODLES_DRAW_BLEND,
             0xffffffffu, 12},
        };
        size_t i;
        printf("\nEngine rates (nominal MB/s counts source and destination bytes the operation implies)\n");
        printf("%-30s %3s %8s %8s %7s %6s %7s\n", "operation", "n", "ms/op", "Mpix/s",
               "ns/px", "clk/px", "MB/s");
        for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) run_rate(&tests[i], dst, reps);
    }

    run_presentation(dst, 10);
    if (info.opcode_mask & NOODLES_CAP_QUEUED_PRESENT) {
        if (noodles_link_enable_three_buffers(link) != 0) fail("three-buffer mode");
        /* Show a cleared buffer C rather than stale memory. */
        for (k = 0; k < 3; ++k) {
            noodles_fence_t fence;
            SUBMIT("back buffer clear", noodles_back_buffer_fill(link, &all, 0xff000000u));
            SUBMIT("present", noodles_push_present(link, &fence));
            confirmed_wait(fence, "present");
        }
        run_presentation(dst, 14);
    }

    printf("\nConfirmed wait on a fill with n full-surface fills queued behind it (ms)\n");
    printf("%2s %7s %7s %7s %7s\n", "n", "raw", "confirm", "min", "max");
    for (k = 0; k <= 8; k += 2) run_confirmation(dst, k);

    noodles_surface_destroy(half);
    noodles_surface_destroy(clear);
    noodles_surface_destroy(opaque);
    noodles_surface_destroy(dst);
    if (noodles_link_close(link, NOODLES_DEFAULT_TIMEOUT_MS) != 0) {
        link = NULL;
        fail("close");
    }
    return 0;
}
