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
typedef int (*sdl_set_render_target_fn)(void*, void*);
typedef int (*sdl_update_texture_fn)(void*, const void*, const void*, int);
typedef int (*sdl_lock_texture_fn)(void*, const void*, void**, int*);
typedef void (*sdl_unlock_texture_fn)(void*);
typedef ssize_t (*read_fn)(int, void*, size_t);
typedef ssize_t (*pread_fn)(int, void*, size_t, off_t);
typedef ssize_t (*pread64_fn)(int, void*, size_t, off64_t);
typedef size_t (*fread_fn)(void*, size_t, size_t, FILE*);
typedef FILE* (*fopen_fn)(const char*, const char*);

static sdl_render_present_fn real_sdl_render_present;
static sdl_delay_fn real_sdl_delay;
static sdl_set_render_target_fn real_sdl_set_render_target;
static sdl_update_texture_fn real_sdl_update_texture;
static sdl_lock_texture_fn real_sdl_lock_texture;
static sdl_unlock_texture_fn real_sdl_unlock_texture;
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
static __thread uint64_t display_boundary;
static __thread uint64_t target_elapsed;
static __thread uint64_t update_elapsed;
static __thread uint64_t lock_elapsed;
static __thread uint64_t unlock_elapsed;
static __thread unsigned target_calls;
static __thread unsigned update_calls;
static __thread unsigned lock_calls;
static __thread unsigned unlock_calls;

static bool resolve_sdl_symbols(void)
{
	if (real_sdl_render_present && real_sdl_delay) return true;
	int saved_guard = trace_guard;
	trace_guard = 1;
	if (!real_sdl_render_present) {
		real_sdl_render_present = (sdl_render_present_fn) dlsym(RTLD_NEXT, "SDL_RenderPresent");
	}
	if (!real_sdl_delay) {
		real_sdl_delay = (sdl_delay_fn) dlsym(RTLD_NEXT, "SDL_Delay");
	}
	if (!real_sdl_set_render_target) {
		real_sdl_set_render_target = (sdl_set_render_target_fn) dlsym(RTLD_NEXT, "SDL_SetRenderTarget");
	}
	if (!real_sdl_update_texture) {
		real_sdl_update_texture = (sdl_update_texture_fn) dlsym(RTLD_NEXT, "SDL_UpdateTexture");
	}
	if (!real_sdl_lock_texture) {
		real_sdl_lock_texture = (sdl_lock_texture_fn) dlsym(RTLD_NEXT, "SDL_LockTexture");
	}
	if (!real_sdl_unlock_texture) {
		real_sdl_unlock_texture = (sdl_unlock_texture_fn) dlsym(RTLD_NEXT, "SDL_UnlockTexture");
	}
	trace_guard = saved_guard;
	return real_sdl_render_present && real_sdl_delay;
}

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

static void report_sdl(const char* operation, uint64_t begin, uint64_t elapsed)
{
	if (elapsed < IO_LIMIT_NS) return;
	trace_line("SDL tid=%ld op=%s at_ms=%.3f elapsed_ms=%.3f\n", (long) current_tid(), operation,
		milliseconds(begin), milliseconds(elapsed));
}

__attribute__((constructor)) static void initialize_trace(void)
{
	trace_guard = 1;
	const char* path = getenv("GEMRB_FRAME_TRACE_LOG");
	if (!path || !*path) path = "/tmp/gemrb-frame-trace.log";
	trace_fd = (int) syscall(SYS_openat, AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);

	real_sdl_render_present = (sdl_render_present_fn) dlsym(RTLD_NEXT, "SDL_RenderPresent");
	real_sdl_delay = (sdl_delay_fn) dlsym(RTLD_NEXT, "SDL_Delay");
	real_sdl_set_render_target = (sdl_set_render_target_fn) dlsym(RTLD_NEXT, "SDL_SetRenderTarget");
	real_sdl_update_texture = (sdl_update_texture_fn) dlsym(RTLD_NEXT, "SDL_UpdateTexture");
	real_sdl_lock_texture = (sdl_lock_texture_fn) dlsym(RTLD_NEXT, "SDL_LockTexture");
	real_sdl_unlock_texture = (sdl_unlock_texture_fn) dlsym(RTLD_NEXT, "SDL_UnlockTexture");
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
	if (!real_sdl_render_present && !resolve_sdl_symbols()) {
		trace_line("TRACE error missing=SDL_RenderPresent\n");
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
	uint64_t boundary = display_boundary;
	uint64_t target_time = target_elapsed;
	uint64_t update_time = update_elapsed;
	uint64_t lock_time = lock_elapsed;
	uint64_t unlock_time = unlock_elapsed;
	unsigned targets = target_calls;
	unsigned updates = update_calls;
	unsigned locks = lock_calls;
	unsigned unlocks = unlock_calls;
	delay_requested = 0;
	delay_elapsed = 0;
	display_boundary = 0;
	target_elapsed = 0;
	update_elapsed = 0;
	lock_elapsed = 0;
	unlock_elapsed = 0;
	target_calls = 0;
	update_calls = 0;
	lock_calls = 0;
	unlock_calls = 0;

	real_sdl_render_present(renderer);
	uint64_t end = monotonic_ns();
	uint64_t present = end - begin;
	uint64_t before_display = boundary && previous_present_end ? boundary - previous_present_end : between;
	uint64_t engine = before_display > delayed ? before_display - delayed : 0;
	uint64_t display = boundary ? begin - boundary : 0;

	if ((interval && interval >= FRAME_LIMIT_NS) || present >= PRESENT_LIMIT_NS) {
		trace_line("FRAME tid=%ld at_ms=%.3f interval_ms=%.3f between_ms=%.3f delay_req_ms=%.3f delay_actual_ms=%.3f engine_ms=%.3f display_ms=%.3f present_ms=%.3f target_ms=%.3f/%u update_ms=%.3f/%u lock_ms=%.3f/%u unlock_ms=%.3f/%u\n",
			(long) current_tid(), milliseconds(begin), milliseconds(interval), milliseconds(between), milliseconds(requested),
			milliseconds(delayed), milliseconds(engine), milliseconds(display), milliseconds(present),
			milliseconds(target_time), targets, milliseconds(update_time), updates, milliseconds(lock_time), locks,
			milliseconds(unlock_time), unlocks);
	}
	previous_present_begin = begin;
	previous_present_end = end;
}

int SDL_SetRenderTarget(void* renderer, void* texture)
{
	if (!real_sdl_set_render_target) resolve_sdl_symbols();
	if (!real_sdl_set_render_target) {
		trace_line("TRACE error missing=SDL_SetRenderTarget\n");
		errno = ENOSYS;
		return -1;
	}
	if (trace_guard || !is_main_thread()) return real_sdl_set_render_target(renderer, texture);
	uint64_t begin = monotonic_ns();
	int result = real_sdl_set_render_target(renderer, texture);
	uint64_t elapsed = monotonic_ns() - begin;
	target_elapsed += elapsed;
	target_calls++;
	if (!texture && !display_boundary) display_boundary = begin;
	report_sdl("set-target", begin, elapsed);
	return result;
}

int SDL_UpdateTexture(void* texture, const void* rect, const void* pixels, int pitch)
{
	if (!real_sdl_update_texture) resolve_sdl_symbols();
	if (!real_sdl_update_texture) {
		trace_line("TRACE error missing=SDL_UpdateTexture\n");
		errno = ENOSYS;
		return -1;
	}
	if (trace_guard || !is_main_thread()) return real_sdl_update_texture(texture, rect, pixels, pitch);
	uint64_t begin = monotonic_ns();
	int result = real_sdl_update_texture(texture, rect, pixels, pitch);
	uint64_t elapsed = monotonic_ns() - begin;
	update_elapsed += elapsed;
	update_calls++;
	report_sdl("update", begin, elapsed);
	return result;
}

int SDL_LockTexture(void* texture, const void* rect, void** pixels, int* pitch)
{
	if (!real_sdl_lock_texture) resolve_sdl_symbols();
	if (!real_sdl_lock_texture) {
		trace_line("TRACE error missing=SDL_LockTexture\n");
		errno = ENOSYS;
		return -1;
	}
	if (trace_guard || !is_main_thread()) return real_sdl_lock_texture(texture, rect, pixels, pitch);
	uint64_t begin = monotonic_ns();
	int result = real_sdl_lock_texture(texture, rect, pixels, pitch);
	uint64_t elapsed = monotonic_ns() - begin;
	lock_elapsed += elapsed;
	lock_calls++;
	report_sdl("lock", begin, elapsed);
	return result;
}

void SDL_UnlockTexture(void* texture)
{
	if (!real_sdl_unlock_texture) resolve_sdl_symbols();
	if (!real_sdl_unlock_texture) {
		trace_line("TRACE error missing=SDL_UnlockTexture\n");
		return;
	}
	if (trace_guard || !is_main_thread()) {
		real_sdl_unlock_texture(texture);
		return;
	}
	uint64_t begin = monotonic_ns();
	real_sdl_unlock_texture(texture);
	uint64_t elapsed = monotonic_ns() - begin;
	unlock_elapsed += elapsed;
	unlock_calls++;
	report_sdl("unlock", begin, elapsed);
}

void SDL_Delay(uint32_t duration_ms)
{
	if (!real_sdl_delay && !resolve_sdl_symbols()) {
		trace_line("TRACE error missing=SDL_Delay\n");
		return;
	}
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
