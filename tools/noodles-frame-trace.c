#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*
 * Temporary LD_PRELOAD tracer for MiSTer GemRB frame stalls.  It writes only
 * long frames, slow renderer presents and slow file operations.  The output
 * goes to tmpfs by default so tracing cannot add SD-card traffic of its own.
 */

#define FRAME_LIMIT_NS (45ULL * 1000ULL * 1000ULL)
#define PRESENT_LIMIT_NS (10ULL * 1000ULL * 1000ULL)
#define IO_LIMIT_NS (4ULL * 1000ULL * 1000ULL)

typedef void (*sdl_render_present_fn)(void*);
typedef void (*sdl_delay_fn)(uint32_t);
typedef ssize_t (*read_fn)(int, void*, size_t);
typedef ssize_t (*pread_fn)(int, void*, size_t, off_t);
typedef ssize_t (*pread64_fn)(int, void*, size_t, off64_t);
typedef size_t (*fread_fn)(void*, size_t, size_t, FILE*);
typedef FILE* (*fopen_fn)(const char*, const char*);

static sdl_render_present_fn real_sdl_render_present;
static sdl_delay_fn real_sdl_delay;
static read_fn real_read;
static pread_fn real_pread;
static pread64_fn real_pread64;
static fread_fn real_fread;
static fopen_fn real_fopen;
static fopen_fn real_fopen64;

static int trace_fd = -1;
static __thread int trace_guard;
static __thread uint64_t previous_present_begin;
static __thread uint64_t previous_present_end;
static __thread uint64_t delay_requested;
static __thread uint64_t delay_elapsed;

static pid_t current_tid(void)
{
	return (pid_t) syscall(SYS_gettid);
}

static bool is_main_thread(void)
{
	return current_tid() == getpid();
}

static uint64_t monotonic_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
}

static double milliseconds(uint64_t ns)
{
	return (double) ns / 1000000.0;
}

static void trace_line(const char* format, ...)
{
	if (trace_fd < 0 || trace_guard) return;
	trace_guard = 1;
	char line[768];
	va_list ap;
	va_start(ap, format);
	int length = vsnprintf(line, sizeof(line), format, ap);
	va_end(ap);
	if (length > 0) {
		size_t count = (size_t) length;
		if (count >= sizeof(line)) count = sizeof(line) - 1;
		(void) syscall(SYS_write, trace_fd, line, count);
	}
	trace_guard = 0;
}

static void fd_name(int fd, char* output, size_t output_size)
{
	if (!output_size) return;
	char link[64];
	int length = snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
	if (length <= 0 || (size_t) length >= sizeof(link)) {
		output[0] = '\0';
		return;
	}
	ssize_t got = syscall(SYS_readlinkat, AT_FDCWD, link, output, output_size - 1);
	if (got < 0) got = 0;
	output[got] = '\0';
}

static void report_io(const char* operation, int fd, size_t requested, ssize_t result, uint64_t elapsed)
{
	if (elapsed < IO_LIMIT_NS) return;
	char path[320];
	fd_name(fd, path, sizeof(path));
	trace_line("IO tid=%ld main=%d op=%s elapsed_ms=%.3f requested=%zu result=%zd fd=%d path=%s\n",
		(long) current_tid(), is_main_thread(), operation, milliseconds(elapsed), requested, result, fd,
		path[0] ? path : "?");
}

__attribute__((constructor)) static void initialize_trace(void)
{
	trace_guard = 1;
	const char* path = getenv("GEMRB_FRAME_TRACE_LOG");
	if (!path || !*path) path = "/tmp/gemrb-frame-trace.log";
	trace_fd = (int) syscall(SYS_openat, AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);

	real_sdl_render_present = (sdl_render_present_fn) dlsym(RTLD_NEXT, "SDL_RenderPresent");
	real_sdl_delay = (sdl_delay_fn) dlsym(RTLD_NEXT, "SDL_Delay");
	real_read = (read_fn) dlsym(RTLD_NEXT, "read");
	real_pread = (pread_fn) dlsym(RTLD_NEXT, "pread");
	real_pread64 = (pread64_fn) dlsym(RTLD_NEXT, "pread64");
	real_fread = (fread_fn) dlsym(RTLD_NEXT, "fread");
	real_fopen = (fopen_fn) dlsym(RTLD_NEXT, "fopen");
	real_fopen64 = (fopen_fn) dlsym(RTLD_NEXT, "fopen64");
	trace_guard = 0;

	trace_line("TRACE start pid=%ld frame_limit_ms=%.0f present_limit_ms=%.0f io_limit_ms=%.0f\n",
		(long) getpid(), milliseconds(FRAME_LIMIT_NS), milliseconds(PRESENT_LIMIT_NS), milliseconds(IO_LIMIT_NS));
}

__attribute__((destructor)) static void finish_trace(void)
{
	trace_line("TRACE stop pid=%ld\n", (long) getpid());
	if (trace_fd >= 0) {
		(void) syscall(SYS_close, trace_fd);
		trace_fd = -1;
	}
}

void SDL_RenderPresent(void* renderer)
{
	if (!real_sdl_render_present) {
		errno = ENOSYS;
		return;
	}
	if (trace_guard || !is_main_thread()) {
		real_sdl_render_present(renderer);
		return;
	}

	uint64_t begin = monotonic_ns();
	uint64_t interval = previous_present_begin ? begin - previous_present_begin : 0;
	uint64_t between = previous_present_end ? begin - previous_present_end : 0;
	uint64_t requested = delay_requested;
	uint64_t delayed = delay_elapsed;
	delay_requested = 0;
	delay_elapsed = 0;

	real_sdl_render_present(renderer);
	uint64_t end = monotonic_ns();
	uint64_t present = end - begin;

	if ((interval && interval >= FRAME_LIMIT_NS) || present >= PRESENT_LIMIT_NS) {
		trace_line("FRAME tid=%ld interval_ms=%.3f between_ms=%.3f delay_req_ms=%.3f delay_actual_ms=%.3f present_ms=%.3f\n",
			(long) current_tid(), milliseconds(interval), milliseconds(between), milliseconds(requested),
			milliseconds(delayed), milliseconds(present));
	}
	previous_present_begin = begin;
	previous_present_end = end;
}

void SDL_Delay(uint32_t duration_ms)
{
	if (!real_sdl_delay) return;
	if (trace_guard || !is_main_thread()) {
		real_sdl_delay(duration_ms);
		return;
	}
	uint64_t begin = monotonic_ns();
	real_sdl_delay(duration_ms);
	uint64_t elapsed = monotonic_ns() - begin;
	delay_requested += (uint64_t) duration_ms * 1000000ULL;
	delay_elapsed += elapsed;
}

ssize_t read(int fd, void* buffer, size_t count)
{
	if (!real_read) return syscall(SYS_read, fd, buffer, count);
	if (trace_guard) return real_read(fd, buffer, count);
	uint64_t begin = monotonic_ns();
	ssize_t result = real_read(fd, buffer, count);
	report_io("read", fd, count, result, monotonic_ns() - begin);
	return result;
}

#ifndef __USE_FILE_OFFSET64
ssize_t pread(int fd, void* buffer, size_t count, off_t offset)
{
	if (!real_pread) {
		errno = ENOSYS;
		return -1;
	}
	if (trace_guard) return real_pread(fd, buffer, count, offset);
	uint64_t begin = monotonic_ns();
	ssize_t result = real_pread(fd, buffer, count, offset);
	report_io("pread", fd, count, result, monotonic_ns() - begin);
	return result;
}
#endif

ssize_t pread64(int fd, void* buffer, size_t count, off64_t offset)
{
	if (!real_pread64) {
		errno = ENOSYS;
		return -1;
	}
	if (trace_guard) return real_pread64(fd, buffer, count, offset);
	uint64_t begin = monotonic_ns();
	ssize_t result = real_pread64(fd, buffer, count, offset);
	report_io("pread64", fd, count, result, monotonic_ns() - begin);
	return result;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream)
{
	if (!real_fread) return 0;
	if (trace_guard) return real_fread(ptr, size, count, stream);
	uint64_t begin = monotonic_ns();
	size_t result = real_fread(ptr, size, count, stream);
	uint64_t elapsed = monotonic_ns() - begin;
	if (elapsed >= IO_LIMIT_NS) {
		size_t requested = size && count > SIZE_MAX / size ? SIZE_MAX : size * count;
		report_io("fread", fileno(stream), requested, (ssize_t) result, elapsed);
	}
	return result;
}

static FILE* traced_fopen(fopen_fn function, const char* operation, const char* path, const char* mode)
{
	if (!function) {
		errno = ENOSYS;
		return NULL;
	}
	if (trace_guard) return function(path, mode);
	uint64_t begin = monotonic_ns();
	FILE* result = function(path, mode);
	uint64_t elapsed = monotonic_ns() - begin;
	if (elapsed >= IO_LIMIT_NS) {
		trace_line("IO tid=%ld main=%d op=%s elapsed_ms=%.3f result=%d path=%s\n",
			(long) current_tid(), is_main_thread(), operation, milliseconds(elapsed), result ? 1 : 0,
			path ? path : "?");
	}
	return result;
}

#ifndef __USE_FILE_OFFSET64
FILE* fopen(const char* path, const char* mode)
{
	return traced_fopen(real_fopen, "fopen", path, mode);
}
#endif

FILE* fopen64(const char* path, const char* mode)
{
	return traced_fopen(real_fopen64 ? real_fopen64 : real_fopen, "fopen64", path, mode);
}
