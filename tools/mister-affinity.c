/* Preloaded by run.sh: keeps GemRB's main thread on its own CPUs while every
   other thread may use the whole process affinity mask.

   The MiSTer frontend keeps CPU 1 nearly busy. GemRB's main thread runs
   best on CPU 0 alone, but its audio and helper threads run best when they
   may also use CPU 1. Threads inherit the mask of the thread that creates
   them, so pinning the main thread alone would confine every later thread
   too. At load time this library records the process mask set by run.sh,
   pins the main thread to MISTER_MAIN_CPUS (default 0) and wraps
   pthread_create so each new thread restores the recorded mask before its
   start routine runs. It reports the masks once and does nothing else. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*create_fn)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);

struct start {
    void *(*routine)(void *);
    void *arg;
};

static create_fn real_create;
static cpu_set_t others;
static int split;

/* Parses a taskset-style list such as "0", "0-1" or "0,1". */
static int parse_cpus(const char *s, cpu_set_t *set)
{
    CPU_ZERO(set);
    while (*s) {
        char *end;
        long lo = strtol(s, &end, 10), hi = lo;
        if (end == s || lo < 0 || lo >= CPU_SETSIZE) return -1;
        s = end;
        if (*s == '-') {
            hi = strtol(s + 1, &end, 10);
            if (end == s + 1 || hi < lo || hi >= CPU_SETSIZE) return -1;
            s = end;
        }
        for (long c = lo; c <= hi; c++) CPU_SET(c, set);
        if (*s == ',') s++;
        else if (*s) return -1;
    }
    return CPU_COUNT(set) ? 0 : -1;
}

static void format_cpus(const cpu_set_t *set, char *out, size_t size)
{
    size_t n = 0;
    out[0] = 0;
    for (int c = 0; c < CPU_SETSIZE && n < size; c++) {
        if (!CPU_ISSET(c, set)) continue;
        int hi = c;
        while (hi + 1 < CPU_SETSIZE && CPU_ISSET(hi + 1, set)) hi++;
        if (hi > c) n += snprintf(out + n, size - n, "%s%d-%d", n ? "," : "", c, hi);
        else n += snprintf(out + n, size - n, "%s%d", n ? "," : "", c);
        c = hi;
    }
}

__attribute__((constructor)) static void pin_main_thread(void)
{
    const char *list = getenv("MISTER_MAIN_CPUS");
    cpu_set_t main_set, both;
    char main_text[64], others_text[64];

    real_create = (create_fn)dlsym(RTLD_NEXT, "pthread_create");
    if (!list || !*list) list = "0";
    if (sched_getaffinity(0, sizeof others, &others)) {
        fprintf(stderr, "mister-affinity: cannot read the process CPU mask; affinity unchanged\n");
        return;
    }
    format_cpus(&others, others_text, sizeof others_text);
    if (parse_cpus(list, &main_set)) {
        fprintf(stderr, "mister-affinity: invalid MISTER_MAIN_CPUS='%s'; all threads keep CPUs %s\n", list, others_text);
        return;
    }
    CPU_AND(&both, &main_set, &others);
    if (!CPU_COUNT(&both)) {
        fprintf(stderr, "mister-affinity: MISTER_MAIN_CPUS='%s' is outside CPUs %s; all threads keep them\n", list, others_text);
        return;
    }
    if (sched_setaffinity(0, sizeof both, &both)) {
        fprintf(stderr, "mister-affinity: cannot pin the main thread; all threads keep CPUs %s\n", others_text);
        return;
    }
    split = !CPU_EQUAL(&both, &others);
    format_cpus(&both, main_text, sizeof main_text);
    fprintf(stderr, "mister-affinity: main thread CPUs %s, other threads CPUs %s\n", main_text, others_text);
}

static void *start_thread(void *p)
{
    struct start s = *(struct start *)p;
    free(p);
    sched_setaffinity(0, sizeof others, &others);
    return s.routine(s.arg);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg)
{
    if (!real_create) real_create = (create_fn)dlsym(RTLD_NEXT, "pthread_create");
    struct start *s = split ? malloc(sizeof *s) : NULL;
    if (!s) return real_create(thread, attr, routine, arg);
    s->routine = routine;
    s->arg = arg;
    int rc = real_create(thread, attr, start_thread, s);
    if (rc) free(s);
    return rc;
}
