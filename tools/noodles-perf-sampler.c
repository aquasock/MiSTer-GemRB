#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DATA_PAGES 64u
#define SAMPLE_SLOTS 65536u

typedef struct {
    uint64_t ip;
    uint64_t count;
} SampleCount;

static volatile sig_atomic_t stop_requested;

static void on_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int perf_event_open(struct perf_event_attr *attr, pid_t pid)
{
    return (int)syscall(__NR_perf_event_open, attr, pid, -1, -1,
                        PERF_FLAG_FD_CLOEXEC);
}

static uint64_t monotonic_ns(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

static uint64_t hash_ip(uint64_t ip)
{
    ip ^= ip >> 33;
    ip *= 0xff51afd7ed558ccdull;
    ip ^= ip >> 33;
    return ip;
}

static int add_sample(SampleCount *samples, uint64_t ip)
{
    size_t slot = (size_t)(hash_ip(ip) & (SAMPLE_SLOTS - 1u));
    size_t checked;
    if (!ip) {
        return 0;
    }
    for (checked = 0; checked < SAMPLE_SLOTS; ++checked) {
        if (!samples[slot].ip || samples[slot].ip == ip) {
            samples[slot].ip = ip;
            samples[slot].count++;
            return 0;
        }
        slot = (slot + 1u) & (SAMPLE_SLOTS - 1u);
    }
    errno = ENOSPC;
    return -1;
}

static void ring_copy(const uint8_t *ring, size_t ring_size, uint64_t offset,
                      void *destination, size_t length)
{
    size_t first = ring_size - (size_t)(offset & (ring_size - 1u));
    if (first > length) {
        first = length;
    }
    memcpy(destination, ring + (offset & (ring_size - 1u)), first);
    if (first < length) {
        memcpy((uint8_t *)destination + first, ring, length - first);
    }
}

static int drain_ring(struct perf_event_mmap_page *metadata, size_t page_size,
                      SampleCount *samples, uint64_t *total, uint64_t *lost)
{
    const size_t ring_size = DATA_PAGES * page_size;
    const uint8_t *ring = (const uint8_t *)metadata + page_size;
    uint64_t head = metadata->data_head;
    uint64_t tail;
    __sync_synchronize();
    tail = metadata->data_tail;

    while (tail < head) {
        struct perf_event_header header;
        uint8_t record[256];
        if (head - tail < sizeof(header)) {
            break;
        }
        ring_copy(ring, ring_size, tail, &header, sizeof(header));
        if (header.size < sizeof(header) || header.size > sizeof(record) ||
            header.size > head - tail) {
            fprintf(stderr, "invalid perf record size %u\n", header.size);
            return -1;
        }
        ring_copy(ring, ring_size, tail, record, header.size);
        if (header.type == PERF_RECORD_SAMPLE) {
            uint64_t ip;
            memcpy(&ip, record + sizeof(header), sizeof(ip));
            if (add_sample(samples, ip) < 0) {
                perror("sample table");
                return -1;
            }
            (*total)++;
        } else if (header.type == PERF_RECORD_LOST &&
                   header.size >= sizeof(header) + sizeof(uint64_t) * 2u) {
            uint64_t count;
            memcpy(&count, record + sizeof(header) + sizeof(uint64_t),
                   sizeof(count));
            *lost += count;
        }
        tail += header.size;
    }
    metadata->data_tail = tail;
    return 0;
}

static int write_maps(pid_t pid)
{
    char path[64];
    char line[1024];
    FILE *maps;
    snprintf(path, sizeof(path), "/proc/%ld/maps", (long)pid);
    maps = fopen(path, "r");
    if (!maps) {
        return -1;
    }
    puts("MAPS_BEGIN");
    while (fgets(line, sizeof(line), maps)) {
        fputs(line, stdout);
    }
    puts("MAPS_END");
    fclose(maps);
    return 0;
}

static int compare_samples(const void *left, const void *right)
{
    const SampleCount *a = (const SampleCount *)left;
    const SampleCount *b = (const SampleCount *)right;
    if (a->count < b->count) return 1;
    if (a->count > b->count) return -1;
    if (a->ip < b->ip) return -1;
    if (a->ip > b->ip) return 1;
    return 0;
}

int main(int argc, char **argv)
{
    struct perf_event_attr attr;
    struct perf_event_mmap_page *metadata = MAP_FAILED;
    SampleCount *samples = NULL;
    SampleCount *sorted = NULL;
    struct pollfd pollfd;
    uint64_t total = 0;
    uint64_t lost = 0;
    uint64_t start;
    uint64_t until;
    size_t unique = 0;
    size_t i;
    long page_size = 0;
    long duration;
    long frequency = 499;
    pid_t pid;
    int fd = -1;
    int result = 1;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s PID SECONDS [FREQUENCY]\n", argv[0]);
        return 2;
    }
    pid = (pid_t)strtol(argv[1], NULL, 10);
    duration = strtol(argv[2], NULL, 10);
    if (argc == 4) {
        frequency = strtol(argv[3], NULL, 10);
    }
    if (pid <= 0 || duration <= 0 || duration > 300 || frequency <= 0 ||
        frequency > 4000) {
        fprintf(stderr, "invalid PID, duration or frequency\n");
        return 2;
    }

    samples = (SampleCount *)calloc(SAMPLE_SLOTS, sizeof(*samples));
    sorted = (SampleCount *)calloc(SAMPLE_SLOTS, sizeof(*sorted));
    if (!samples || !sorted) {
        perror("calloc");
        goto done;
    }

    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_TASK_CLOCK;
    attr.sample_freq = (uint64_t)frequency;
    attr.freq = 1;
    attr.sample_type = PERF_SAMPLE_IP;
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.wakeup_events = 1;

    fd = perf_event_open(&attr, pid);
    if (fd < 0) {
        fprintf(stderr, "perf_event_open for PID %ld: %s\n", (long)pid,
                strerror(errno));
        goto done;
    }
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        fprintf(stderr, "cannot determine page size\n");
        goto done;
    }
    metadata = (struct perf_event_mmap_page *)mmap(
        NULL, (DATA_PAGES + 1u) * (size_t)page_size, PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);
    if (metadata == MAP_FAILED) {
        perror("mmap perf ring");
        goto done;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("NOODLES_PERF_V1 pid=%ld seconds=%ld frequency=%ld\n", (long)pid,
           duration, frequency);
    if (write_maps(pid) < 0) {
        perror("process maps");
        goto done;
    }

    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) < 0 ||
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        perror("enable perf event");
        goto done;
    }
    start = monotonic_ns();
    until = start + (uint64_t)duration * 1000000000ull;
    pollfd.fd = fd;
    pollfd.events = POLLIN;
    while (!stop_requested && monotonic_ns() < until) {
        int polled = poll(&pollfd, 1, 100);
        if (polled < 0 && errno != EINTR) {
            perror("poll perf event");
            goto done;
        }
        if (drain_ring(metadata, (size_t)page_size, samples, &total, &lost) < 0) {
            goto done;
        }
        if (kill(pid, 0) < 0 && errno == ESRCH) {
            break;
        }
    }
    (void)ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    if (drain_ring(metadata, (size_t)page_size, samples, &total, &lost) < 0) {
        goto done;
    }

    for (i = 0; i < SAMPLE_SLOTS; ++i) {
        if (samples[i].count) {
            sorted[unique++] = samples[i];
        }
    }
    qsort(sorted, unique, sizeof(*sorted), compare_samples);
    for (i = 0; i < unique; ++i) {
        printf("SAMPLE %016llx %llu\n",
               (unsigned long long)sorted[i].ip,
               (unsigned long long)sorted[i].count);
    }
    printf("SUMMARY samples=%llu lost=%llu unique=%zu elapsed_ms=%llu\n",
           (unsigned long long)total, (unsigned long long)lost, unique,
           (unsigned long long)((monotonic_ns() - start) / 1000000ull));
    result = total ? 0 : 3;

done:
    if (metadata != MAP_FAILED && page_size > 0) {
        munmap(metadata, (DATA_PAGES + 1u) * (size_t)page_size);
    }
    if (fd >= 0) {
        close(fd);
    }
    free(sorted);
    free(samples);
    return result;
}
