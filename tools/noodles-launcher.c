/*
 * Stock-Main launch coordinator for HPS games using MiSTer-Noodles.
 *
 * An OSD script starts this program, which detaches and waits briefly for Main
 * to restart with a Noodles MGL.  The MGL's <noodles> record selects a known
 * engine adapter and its data.  For a valid launch it duplicates Main's open
 * evdev descriptors with pidfd_getfd and changes their exclusive grabs without
 * switching away from the Noodles display.  This program then waits for the
 * foreground adapter and restores Main's grabs on exit.  A temporary uinput
 * keyboard uses Main's framebuffer path only to show validation errors.
 */
#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_WAIT_SECONDS 30
#define LOG_PATH "/media/fat/gemrb/noodles-launch.log"
#define GAME_LOG_PATH "/media/fat/gemrb/noodles-game.log"
#define LOCK_PATH "/tmp/noodles-launcher.lock"
#define CONTROL_NAME "Noodles launcher control"
// The Noodles core is installed under its release name beside the game MGLs:
// _Utility/Noodles_YYYYMMDD.rbf, or an undated Noodles.rbf.
#define CORE_DIR "/media/fat/_Utility/"
#define MAX_MGL_BYTES (64 * 1024)

struct manifest {
    char engine[32];
    char game[32];
    char data[PATH_MAX];
    char require[NAME_MAX + 1];
};

struct main_instance {
    pid_t pid;
    char rbf[PATH_MAX];
    char mgl[PATH_MAX];
};

static FILE *log_file;
static volatile sig_atomic_t stop_requested;
static pid_t adapter_pid = -1;

static int64_t monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(unsigned ms)
{
    struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR && !stop_requested) {}
}

static void log_line(const char *fmt, ...)
{
    char stamp[32];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);
    if (!log_file) return;
    fprintf(log_file, "%s ", stamp);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
    fputc('\n', log_file);
    fflush(log_file);
}

static void close_log(void)
{
    if (log_file) fclose(log_file);
    log_file = NULL;
}

static void trim_log(const char *path)
{
    struct stat st;
    if (stat(path, &st) || st.st_size <= 65536) return;
    FILE *in = fopen(path, "rb");
    if (!in) return;
    const long keep = 32768;
    if (fseek(in, st.st_size - keep, SEEK_SET)) { fclose(in); return; }
    char *buf = malloc((size_t)keep);
    if (!buf) { fclose(in); return; }
    size_t got = fread(buf, 1, (size_t)keep, in);
    fclose(in);
    FILE *out = fopen(path, "wb");
    if (out) {
        fputs("--- older launcher log truncated ---\n", out);
        fwrite(buf, 1, got, out);
        fclose(out);
    }
    free(buf);
}

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
    if (adapter_pid > 0) kill(-adapter_pid, SIGTERM);
}

static bool numeric_name(const char *s)
{
    if (!*s) return false;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return false;
    return true;
}

static int read_cmdline(pid_t pid, char *buf, size_t size, char **args, int max_args)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", (long)pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    int argc = 0;
    for (ssize_t i = 0; i < n && argc < max_args;) {
        args[argc++] = &buf[i];
        while (i < n && buf[i]) i++;
        i++;
    }
    return argc;
}

static bool ends_with_ci(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !strcasecmp(s + a - b, suffix);
}

static bool noodles_rbf(const char *path)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    return !strncasecmp(base, "Noodles", 7) && ends_with_ci(base, ".rbf");
}

static pid_t current_main_pid(void)
{
    DIR *d = opendir("/proc");
    if (!d) return -1;
    pid_t best = -1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!numeric_name(de->d_name)) continue;
        pid_t pid = (pid_t)strtol(de->d_name, NULL, 10);
        char buf[PATH_MAX * 2], *args[4];
        int argc = read_cmdline(pid, buf, sizeof(buf), args, 4);
        if (argc < 1) continue;
        const char *base = strrchr(args[0], '/');
        base = base ? base + 1 : args[0];
        if (!strcmp(base, "MiSTer") && pid > best) best = pid;
    }
    closedir(d);
    return best;
}

static bool find_mgl_main(pid_t initial, struct main_instance *out)
{
    DIR *d = opendir("/proc");
    if (!d) return false;
    bool found = false;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!numeric_name(de->d_name)) continue;
        pid_t pid = (pid_t)strtol(de->d_name, NULL, 10);
        if (pid == initial) continue;
        char buf[PATH_MAX * 3], *args[5];
        int argc = read_cmdline(pid, buf, sizeof(buf), args, 5);
        if (argc < 3) continue;
        const char *base = strrchr(args[0], '/');
        base = base ? base + 1 : args[0];
        if (strcmp(base, "MiSTer") || !noodles_rbf(args[1]) || !ends_with_ci(args[2], ".mgl")) continue;
        out->pid = pid;
        snprintf(out->rbf, sizeof(out->rbf), "%s", args[1]);
        snprintf(out->mgl, sizeof(out->mgl), "%s", args[2]);
        found = true;
        break;
    }
    closedir(d);
    return found;
}

static char *read_small_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) || st.st_size < 1 || st.st_size > MAX_MGL_BYTES) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    if (got != (size_t)st.st_size) { free(buf); return NULL; }
    buf[got] = '\0';
    return buf;
}

static bool extract_tag(const char *begin, const char *limit, const char *tag, char *out, size_t out_size)
{
    char open[64], close[64];
    snprintf(open, sizeof(open), "<%s>", tag);
    snprintf(close, sizeof(close), "</%s>", tag);
    const char *a = strstr(begin, open);
    if (!a || a >= limit) return false;
    a += strlen(open);
    const char *b = strstr(a, close);
    if (!b || b > limit) return false;
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    if (!n || n >= out_size) return false;
    memcpy(out, a, n);
    out[n] = '\0';
    return true;
}

static bool simple_id(const char *s)
{
    if (!*s) return false;
    for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-') return false;
    return true;
}

static bool safe_data_path(const char *s)
{
    if (strncmp(s, "/media/fat/", 11) || strstr(s, "/../") || strstr(s, "/./")) return false;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (!isalnum(*p) && !strchr("/._- ()[]", *p)) return false;
    }
    size_t n = strlen(s);
    return n > 11 && strcmp(s + n - 3, "/..") && strcmp(s + n - 2, "/.");
}

static bool safe_basename(const char *s)
{
    if (!*s || !strcmp(s, ".") || !strcmp(s, "..")) return false;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (!isalnum(*p) && !strchr("._- ()[]", *p)) return false;
    }
    return true;
}

static int parse_manifest(const char *path, struct manifest *m, char *error, size_t error_size)
{
    memset(m, 0, sizeof(*m));
    char *xml = read_small_file(path);
    if (!xml) { snprintf(error, error_size, "cannot read bounded MGL: %s", path); return -1; }
    const char *section = strstr(xml, "<noodles>");
    const char *end = section ? strstr(section, "</noodles>") : NULL;
    if (!section || !end) { snprintf(error, error_size, "MGL has no <noodles> launch record"); free(xml); return -1; }
    section += strlen("<noodles>");
    bool ok = extract_tag(section, end, "engine", m->engine, sizeof(m->engine)) &&
              extract_tag(section, end, "game", m->game, sizeof(m->game)) &&
              extract_tag(section, end, "data", m->data, sizeof(m->data)) &&
              extract_tag(section, end, "require", m->require, sizeof(m->require));
    free(xml);
    if (!ok) { snprintf(error, error_size, "MGL launch record is incomplete or too long"); return -1; }
    if (!simple_id(m->engine) || !simple_id(m->game)) {
        snprintf(error, error_size, "engine and game identifiers must be alphanumeric"); return -1;
    }
    if (!safe_data_path(m->data)) { snprintf(error, error_size, "unsafe game-data path: %s", m->data); return -1; }
    if (!safe_basename(m->require)) {
        snprintf(error, error_size, "required file must be a basename"); return -1;
    }
    return 0;
}

static bool directory_has_ci(const char *dir, const char *name)
{
    DIR *d = opendir(dir);
    if (!d) return false;
    bool found = false;
    struct dirent *de;
    while ((de = readdir(d))) if (!strcasecmp(de->d_name, name)) { found = true; break; }
    closedir(d);
    return found;
}

static int validate_manifest(const struct manifest *m, char *error, size_t error_size)
{
    static const char *games[] = { "bg1", "bg2", "pst", "iwd", "iwd2" };
    char expected_data[PATH_MAX];
    bool game_ok = false;
    if (strcmp(m->engine, "gemrb")) {
        snprintf(error, error_size, "engine '%s' is not installed", m->engine); return -1;
    }
    for (size_t i = 0; i < sizeof(games) / sizeof(games[0]); i++) if (!strcmp(m->game, games[i])) game_ok = true;
    if (!game_ok) { snprintf(error, error_size, "GemRB game '%s' is not registered", m->game); return -1; }
    snprintf(expected_data, sizeof(expected_data), "/media/fat/gemrb/games/%s", m->game);
    if (strcmp(m->data, expected_data)) {
        snprintf(error, error_size, "GemRB data path does not match game '%s': %s", m->game, m->data); return -1;
    }
    if (strcasecmp(m->require, "CHITIN.KEY")) {
        snprintf(error, error_size, "GemRB requires CHITIN.KEY, not '%s'", m->require); return -1;
    }
    struct stat st;
    if (stat(m->data, &st) || !S_ISDIR(st.st_mode)) {
        snprintf(error, error_size, "game-data directory is missing: %s", m->data); return -1;
    }
    if (!directory_has_ci(m->data, m->require)) {
        snprintf(error, error_size, "required game file is missing: %s/%s", m->data, m->require); return -1;
    }
    if (access("/media/fat/gemrb/run.sh", X_OK)) {
        snprintf(error, error_size, "GemRB adapter is missing: /media/fat/gemrb/run.sh"); return -1;
    }
    return 0;
}

static int create_control_keyboard(char *event_path, size_t event_size)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) || ioctl(fd, UI_SET_KEYBIT, KEY_LEFTCTRL) ||
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTALT) || ioctl(fd, UI_SET_KEYBIT, KEY_F9) ||
        ioctl(fd, UI_SET_KEYBIT, KEY_F12)) {
        close(fd); return -1;
    }
    struct uinput_user_dev dev;
    memset(&dev, 0, sizeof(dev));
    snprintf(dev.name, sizeof(dev.name), "%s", CONTROL_NAME);
    dev.id.bustype = BUS_USB;
    dev.id.vendor = 0x1209;
    dev.id.product = 0x4e4c;
    dev.id.version = 1;
    if (write(fd, &dev, sizeof(dev)) != (ssize_t)sizeof(dev) || ioctl(fd, UI_DEV_CREATE)) {
        close(fd); return -1;
    }
    int64_t until = monotonic_ms() + 3000;
    while (monotonic_ms() < until) {
        for (int i = 0; i < 64; i++) {
            char name_path[PATH_MAX], name[128];
            snprintf(name_path, sizeof(name_path), "/sys/class/input/event%d/device/name", i);
            FILE *f = fopen(name_path, "r");
            if (!f) continue;
            if (fgets(name, sizeof(name), f)) name[strcspn(name, "\r\n")] = '\0';
            else name[0] = '\0';
            fclose(f);
            if (!strcmp(name, CONTROL_NAME)) {
                snprintf(event_path, event_size, "/dev/input/event%d", i);
                return fd;
            }
        }
        sleep_ms(50);
    }
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return -1;
}

static bool main_opened_event(pid_t pid, const char *event_path)
{
    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "/proc/%ld/fd", (long)pid);
    DIR *d = opendir(dir_path);
    if (!d) return false;
    bool found = false;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!numeric_name(de->d_name)) continue;
        char link[PATH_MAX], target[PATH_MAX];
        snprintf(link, sizeof(link), "%s/%s", dir_path, de->d_name);
        ssize_t n = readlink(link, target, sizeof(target) - 1);
        if (n < 0) continue;
        target[n] = '\0';
        if (!strcmp(target, event_path)) { found = true; break; }
    }
    closedir(d);
    return found;
}

static int emit_key(int fd, uint16_t code, int value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) return -1;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    return write(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev) ? 0 : -1;
}

static int send_handoff_key(int fd)
{
    /* An MGL leaves Main's OSD closed, where F9 is forwarded to the core.
     * Open the OSD first so Main consumes the framebuffer hotkey itself. */
    if (emit_key(fd, KEY_F12, 1)) return -1;
    sleep_ms(100);
    if (emit_key(fd, KEY_F12, 0)) return -1;
    sleep_ms(500);
    if (emit_key(fd, KEY_LEFTCTRL, 1)) return -1;
    sleep_ms(40);
    if (emit_key(fd, KEY_LEFTALT, 1)) return -1;
    sleep_ms(40);
    if (emit_key(fd, KEY_F9, 1)) return -1;
    /* Main exposes one debounced menu-key slot. Keep F9 there long enough for
     * its 20 ms debounce instead of immediately overwriting it with releases. */
    sleep_ms(100);
    if (emit_key(fd, KEY_F9, 0)) return -1;
    sleep_ms(40);
    if (emit_key(fd, KEY_LEFTALT, 0)) return -1;
    sleep_ms(40);
    if (emit_key(fd, KEY_LEFTCTRL, 0)) return -1;
    return 0;
}

static int send_restore_key(int fd)
{
    /* While Main's framebuffer terminal is active, F12 returns to the core
     * and video_fb_enable(0) restores Main's physical input grabs. */
    if (emit_key(fd, KEY_F12, 1)) return -1;
    sleep_ms(100);
    if (emit_key(fd, KEY_F12, 0)) return -1;
    return 0;
}

/* Returns 1 when Main still owns at least one physical event node, 0 when all
 * testable physical nodes are free, and -1 when no physical node was found. */
static int physical_input_busy(void)
{
    int found = 0, busy = 0;
    for (int i = 0; i < 64; i++) {
        char path[64], name[128] = {};
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (!strcmp(name, CONTROL_NAME) || !strcmp(name, "MiSTer virtual input")) { close(fd); continue; }
        found++;
        if (!ioctl(fd, EVIOCGRAB, 1)) ioctl(fd, EVIOCGRAB, 0);
        else busy++;
        close(fd);
    }
    return !found ? -1 : !!busy;
}

static int handoff_main_framebuffer(pid_t pid, int *uinput_fd)
{
    char event_path[64] = {};
    int fd = create_control_keyboard(event_path, sizeof(event_path));
    if (fd < 0) { log_line("handoff: cannot create virtual keyboard: %s", strerror(errno)); return -1; }
    int64_t until = monotonic_ms() + 5000;
    while (monotonic_ms() < until && !main_opened_event(pid, event_path) && !stop_requested) sleep_ms(50);
    if (!main_opened_event(pid, event_path)) {
        log_line("handoff: Main %ld did not open %s", (long)pid, event_path);
        ioctl(fd, UI_DEV_DESTROY); close(fd); return -1;
    }
    if (send_handoff_key(fd)) {
        log_line("handoff: virtual key write failed: %s", strerror(errno));
        ioctl(fd, UI_DEV_DESTROY); close(fd); return -1;
    }
    until = monotonic_ms() + 3000;
    int busy;
    do { sleep_ms(50); busy = physical_input_busy(); } while (busy == 1 && monotonic_ms() < until && !stop_requested);
    if (busy != 0) {
        log_line("handoff: physical input remained grabbed (probe=%d)", busy);
        ioctl(fd, UI_DEV_DESTROY); close(fd); return -1;
    }
    log_line("handoff: Main %ld released physical input via F12 and Ctrl-Alt-F9", (long)pid);
    *uinput_fd = fd;
    return 0;
}

static void restore_main_framebuffer(int uinput_fd)
{
    if (uinput_fd < 0) return;
    int busy = physical_input_busy();
    if (busy == 0) {
        send_restore_key(uinput_fd);
        int64_t until = monotonic_ms() + 3000;
        do { sleep_ms(50); busy = physical_input_busy(); } while (busy == 0 && monotonic_ms() < until);
        log_line("restore: Main input probe=%d", busy);
    } else {
        log_line("restore: Main already owns input (probe=%d)", busy);
    }
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
}

static void show_launch_error(pid_t pid, const char *message)
{
    int uinput_fd = -1;
    if (handoff_main_framebuffer(pid, &uinput_fd)) return;
    int tty = open("/dev/tty1", O_WRONLY | O_NOCTTY | O_CLOEXEC);
    if (tty >= 0) {
        dprintf(tty, "\033[2J\033[H\n  Noodles launch failed\n\n  %s\n\n  Returning to MiSTer in 10 seconds.\n", message);
        close(tty);
    }
    for (int i = 0; i < 100 && !stop_requested; i++) sleep_ms(100);
    restore_main_framebuffer(uinput_fd);
}

static bool event_target(const char *path)
{
    static const char prefix[] = "/dev/input/event";
    if (strncmp(path, prefix, sizeof(prefix) - 1)) return false;
    return numeric_name(path + sizeof(prefix) - 1);
}

static int set_main_input_grabs(pid_t pid, int grab)
{
    int pidfd = (int)syscall(SYS_pidfd_open, pid, 0);
    if (pidfd < 0) {
        log_line("input: pidfd_open Main %ld failed: %s", (long)pid, strerror(errno));
        return -1;
    }

    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "/proc/%ld/fd", (long)pid);
    DIR *d = opendir(dir_path);
    if (!d) {
        log_line("input: cannot inspect Main %ld descriptors: %s", (long)pid, strerror(errno));
        close(pidfd);
        return -1;
    }

    int found = 0, changed = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!numeric_name(de->d_name)) continue;
        char link[PATH_MAX], target[PATH_MAX];
        snprintf(link, sizeof(link), "%s/%s", dir_path, de->d_name);
        ssize_t n = readlink(link, target, sizeof(target) - 1);
        if (n < 0) continue;
        target[n] = '\0';
        if (!event_target(target)) continue;
        found++;

        int main_fd = (int)strtol(de->d_name, NULL, 10);
        int fd = (int)syscall(SYS_pidfd_getfd, pidfd, main_fd, 0);
        if (fd < 0) {
            log_line("input: pidfd_getfd Main %ld fd %d failed: %s", (long)pid, main_fd, strerror(errno));
            continue;
        }
        if (!ioctl(fd, EVIOCGRAB, grab)) changed++;
        else log_line("input: EVIOCGRAB(%d) %s failed: %s", grab, target, strerror(errno));
        close(fd);
    }
    closedir(d);
    close(pidfd);

    if (!found || changed != found) {
        log_line("input: Main %ld grab=%d changed=%d found=%d", (long)pid, grab, changed, found);
        return -1;
    }
    log_line("input: Main %ld grab=%d changed=%d", (long)pid, grab, changed);
    return 0;
}

static int release_main_input(pid_t pid)
{
    if (set_main_input_grabs(pid, 0)) return -1;
    int64_t until = monotonic_ms() + 3000;
    int busy;
    do { sleep_ms(50); busy = physical_input_busy(); } while (busy == 1 && monotonic_ms() < until && !stop_requested);
    if (busy != 0) {
        log_line("input: physical input remained grabbed after direct release (probe=%d)", busy);
        return -1;
    }
    log_line("input: Main %ld released physical input without framebuffer switch", (long)pid);
    return 0;
}

static void restore_main_input(pid_t pid)
{
    if (kill(pid, 0)) {
        log_line("input: Main %ld disappeared before grab restore", (long)pid);
        return;
    }
    if (!set_main_input_grabs(pid, 1)) log_line("input: Main %ld physical grabs restored", (long)pid);
}

// Accept only the release-named core in CORE_DIR.
static int release_core(const char *path)
{
    size_t dir_len = strlen(CORE_DIR);
    if (strncmp(path, CORE_DIR, dir_len)) return 0;
    const char *base = path + dir_len;
    if (!strcasecmp(base, "Noodles.rbf")) return 1;
    if (strncasecmp(base, "Noodles_", 8)) return 0;
    for (int i = 8; i < 16; i++)
        if (!isdigit((unsigned char)base[i])) return 0;
    return !strcasecmp(base + 16, ".rbf");
}

static int run_adapter(const struct manifest *m)
{
    adapter_pid = fork();
    if (adapter_pid < 0) return -1;
    if (!adapter_pid) {
        setpgid(0, 0);
        int fd = open(GAME_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (fd >= 0) { dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd); }
        setenv("SDL_RENDER_DRIVER", "noodles", 1);
        setenv("MISTER_RESOLUTION", "800x600", 1);
        setenv("MISTER_OUTPUT_MODE", "off", 1);
        setenv("MISTER_SWAP", "none", 1);
        setenv("NOODLES_ENGINE", m->engine, 1);
        setenv("NOODLES_GAME", m->game, 1);
        setenv("NOODLES_DATA", m->data, 1);
        execl("/media/fat/gemrb/run.sh", "/media/fat/gemrb/run.sh", m->game, (char *)NULL);
        _exit(127);
    }
    setpgid(adapter_pid, adapter_pid);
    int status = 0;
    while (waitpid(adapter_pid, &status, 0) < 0) {
        if (errno != EINTR) { status = -1; break; }
        if (stop_requested) kill(-adapter_pid, SIGTERM);
    }
    adapter_pid = -1;
    if (status < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

static int inspect_mgl(const char *path)
{
    struct manifest m;
    char error[PATH_MAX + NAME_MAX + 256];
    if (parse_manifest(path, &m, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
    printf("engine=%s\ngame=%s\ndata=%s\nrequire=%s\n", m.engine, m.game, m.data, m.require);
    return 0;
}

static int coordinator(unsigned wait_seconds)
{
    int lock_fd = open(LOCK_PATH, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB)) return 2;
    pid_t initial = current_main_pid();
    trim_log(LOG_PATH);
    log_file = fopen(LOG_PATH, "a");
    log_line("watch: started initial_main=%ld timeout=%u", (long)initial, wait_seconds);

    struct main_instance mi = {};
    int64_t until = monotonic_ms() + (int64_t)wait_seconds * 1000;
    while (!stop_requested && monotonic_ms() < until) {
        if (find_mgl_main(initial, &mi)) break;
        sleep_ms(100);
    }
    if (!mi.pid) {
        log_line("watch: timeout");
        close_log();
        close(lock_fd);
        return stop_requested ? 130 : 0;
    }
    sleep_ms(500);
    if (kill(mi.pid, 0)) {
        log_line("watch: selected Main disappeared");
        close_log(); close(lock_fd); return 3;
    }
    log_line("watch: Main=%ld rbf=%s mgl=%s", (long)mi.pid, mi.rbf, mi.mgl);

    struct manifest m;
    char error[PATH_MAX + NAME_MAX + 256];
    if (!release_core(mi.rbf)) {
        snprintf(error, sizeof(error), "unsupported Noodles RBF: %s", mi.rbf);
        log_line("reject: %s", error);
        show_launch_error(mi.pid, error);
        close_log(); close(lock_fd); return 4;
    }
    if (parse_manifest(mi.mgl, &m, error, sizeof(error)) || validate_manifest(&m, error, sizeof(error))) {
        log_line("reject: %s", error);
        show_launch_error(mi.pid, error);
        close_log(); close(lock_fd); return 4;
    }
    log_line("validated: engine=%s game=%s data=%s require=%s", m.engine, m.game, m.data, m.require);

    if (release_main_input(mi.pid)) {
        close_log(); close(lock_fd); return 5;
    }
    int rc = run_adapter(&m);
    log_line("adapter: engine=%s game=%s exit=%d", m.engine, m.game, rc);
    restore_main_input(mi.pid);
    log_line("watch: finished");
    close_log();
    close(lock_fd);
    return rc;
}

static int detach_and_run(unsigned wait_seconds)
{
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid > 0) return 0;
    if (setsid() < 0) _exit(1);
    int nullfd = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (nullfd >= 0) {
        dup2(nullfd, STDIN_FILENO);
        dup2(nullfd, STDOUT_FILENO);
        dup2(nullfd, STDERR_FILENO);
        if (nullfd > STDERR_FILENO) close(nullfd);
    }
    _exit(coordinator(wait_seconds));
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--check-mgl")) return inspect_mgl(argv[2]);
    unsigned wait_seconds = DEFAULT_WAIT_SECONDS;
    bool foreground = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--foreground")) foreground = true;
        else if (!strcmp(argv[i], "--wait") && i + 1 < argc) {
            char *end;
            unsigned long n = strtoul(argv[++i], &end, 10);
            if (*end || n < 1 || n > 300) { fprintf(stderr, "invalid wait interval\n"); return 2; }
            wait_seconds = (unsigned)n;
        } else { fprintf(stderr, "usage: %s [--foreground] [--wait 1..300] | --check-mgl PATH\n", argv[0]); return 2; }
    }
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    return foreground ? coordinator(wait_seconds) : detach_and_run(wait_seconds);
}
