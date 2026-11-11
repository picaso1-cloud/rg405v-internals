// dpadkeys.c — make the controller D-pad usable in Android UI.
//
// GammaOS forwards the D-pad only as ABS_HAT0X/Y. Games read that fine, but
// AOSP phone Settings (and many list UIs) navigate with Tab / Enter, not with
// arrow keys. This daemon watches the pad and injects a keyboard:
//
//   D-pad Down  -> TAB            (next row/item)
//   D-pad Up    -> SHIFT + TAB    (previous row/item)
//   D-pad Left  -> KEY_LEFT
//   D-pad Right -> KEY_RIGHT
//   A           -> ENTER          (activate)
//   B           -> BACK
//
// Hold Down/Up to repeat.
//
// Config: /data/local/tmp/dpadkeys.conf
//   mode=tab|arrows        (default tab)
//   enter_on_a=1
//   back_on_b=1
//
// Build: aarch64-linux-gnu-gcc -O2 -static -o dpadkeys dpadkeys.c

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define NBITS(x)      ((((x) - 1) / BITS_PER_LONG) + 1)
#define TEST_BIT(bit, array) \
    ((array)[(bit) / BITS_PER_LONG] & (1UL << ((bit) % BITS_PER_LONG)))

#define TICK_MS     8
#define REPEAT_MS   700

static volatile sig_atomic_t running = 1;
static void on_signal(int s) { (void)s; running = 0; }

static int cfg_tab_mode = 1;
static int cfg_enter_a = 1;
static int cfg_back_b  = 1;
static int cfg_a_east  = 1;   /* A label is the east face button */

/* ------------------------------------------------------------------ */

static const char *devname(int fd) {
    static char n[256];
    memset(n, 0, sizeof(n));
    ioctl(fd, EVIOCGNAME(sizeof(n) - 1), n);
    return n;
}

static int score(int fd) {
    const char *n = devname(fd);
    if (!n[0] || strcasestr(n, "mouse")) return 0;
    unsigned long absb[NBITS(ABS_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absb)), absb) < 0) return 0;
    if (!TEST_BIT(ABS_HAT0X, absb) && !TEST_BIT(ABS_HAT0Y, absb)) return 0;
    int s = 1;
    if (strcasestr(n, "Xbox Wireless Controller")) s += 4;
    if (strcasestr(n, "GammaPad")) s += 3;
    if (strcasestr(n, "joypad")) s += 3;
    return s;
}

static int find_pad(char *out, size_t len) {
    DIR *d = opendir("/dev/input");
    if (!d) return -1;
    struct dirent *de;
    int best = 0;
    while ((de = readdir(d))) {
        if (strncmp(de->d_name, "event", 5)) continue;
        char p[300];
        snprintf(p, sizeof(p), "/dev/input/%s", de->d_name);
        int fd = open(p, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        int s = score(fd);
        if (s > best) { best = s; snprintf(out, len, "%s", p); }
        close(fd);
    }
    closedir(d);
    return best > 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */

static void load_config(void) {
    FILE *f = fopen("/data/local/tmp/dpadkeys.conf", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = line, *v = eq + 1;
        while (*k == ' ' || *k == '\t') k++;
        char *e = k + strlen(k);
        while (e > k && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = '\0';
        while (*v == ' ' || *v == '\t') v++;
        e = v + strlen(v);
        while (e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = '\0';
        if (!strcmp(k, "mode")) cfg_tab_mode = strcmp(v, "arrows") != 0;
        else if (!strcmp(k, "enter_on_a")) cfg_enter_a = atoi(v);
        else if (!strcmp(k, "back_on_b")) cfg_back_b = atoi(v);
        else if (!strcmp(k, "a_east")) cfg_a_east = atoi(v);
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */

static int create_keyboard(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    int keys[] = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_TAB,
                   KEY_LEFTSHIFT, KEY_ENTER, KEY_BACK };
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        ioctl(fd, UI_SET_KEYBIT, keys[i]);
    ioctl(fd, UI_SET_EVBIT, EV_REP);

    struct uinput_setup us;
    memset(&us, 0, sizeof(us));
    us.id.bustype = BUS_VIRTUAL;
    us.id.vendor = 0x1209;
    us.id.product = 0x0003;
    us.id.version = 2;
    snprintf(us.name, sizeof(us.name), "andr36oid-dpad");

    if (ioctl(fd, UI_DEV_SETUP, &us) < 0) { close(fd); return -1; }
    if (ioctl(fd, UI_DEV_CREATE) < 0) { close(fd); return -1; }
    return fd;
}

static void emit_key(int fd, int code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY; ev.code = code; ev.value = value;
    write(fd, &ev, sizeof(ev));
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SYN; ev.code = SYN_REPORT;
    write(fd, &ev, sizeof(ev));
}

static void tap_key(int fd, int code) {
    emit_key(fd, code, 1);
    emit_key(fd, code, 0);
}

static void shift_tab(int fd) {
    emit_key(fd, KEY_LEFTSHIFT, 1);
    tap_key(fd, KEY_TAB);
    emit_key(fd, KEY_LEFTSHIFT, 0);
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    char path[256] = {0};
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-d") && i + 1 < argc) snprintf(path, sizeof(path), "%s", argv[++i]);

    load_config();

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (!path[0] && find_pad(path, sizeof(path)) < 0) {
        fprintf(stderr, "dpadkeys: no pad with a hat axis found\n");
        return 1;
    }

    int pad = open(path, O_RDONLY | O_NONBLOCK);
    if (pad < 0) { perror(path); return 1; }
    fprintf(stderr, "dpadkeys: source %s (%s)\n", path, devname(pad));

    int kb = create_keyboard();
    if (kb < 0) { perror("/dev/uinput"); return 1; }
    fprintf(stderr, "dpadkeys: virtual keyboard ready (mode=%s enter_a=%d back_b=%d a_east=%d)\n",
            cfg_tab_mode ? "tab" : "arrows", cfg_enter_a, cfg_back_b, cfg_a_east);

    int hx = 0, hy = 0;
    long long last_move = 0;
    long long last_tab = 0;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(pad, &fds);
        struct timeval tv = {0, TICK_MS * 1000};
        int r = select(pad + 1, &fds, NULL, NULL, &tv);
        if (r < 0 && errno != EINTR) break;

        if (r > 0 && FD_ISSET(pad, &fds)) {
            struct input_event ev;
            while (read(pad, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type == EV_KEY) {
                    if (ev.value == 1) {
                        int enter_btn = cfg_a_east ? BTN_EAST : BTN_SOUTH;
                        int back_btn  = cfg_a_east ? BTN_SOUTH : BTN_EAST;
                        if (ev.code == enter_btn && cfg_enter_a) {
                            fprintf(stderr, "btn=%d -> ENTER\n", ev.code);
                            tap_key(kb, KEY_ENTER);
                        }
                        if (ev.code == back_btn && cfg_back_b) {
                            fprintf(stderr, "btn=%d -> BACK\n", ev.code);
                            tap_key(kb, KEY_BACK);
                        }
                    }
                    continue;
                }
                if (ev.type != EV_ABS) continue;
                int nhx = hx, nhy = hy;
                if (ev.code == ABS_HAT0X) nhx = ev.value;
                else if (ev.code == ABS_HAT0Y) nhy = ev.value;
                else continue;

                /* gammapad re-sends the same hat value; only act on a change */
                if (nhx == hx && nhy == hy) continue;
                hx = nhx; hy = nhy;

                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long long ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;

                if (hy < 0) {
                    if (ms - last_tab < 200) continue;
                    last_tab = ms;
                    if (cfg_tab_mode) { fprintf(stderr, "TAB back\n"); shift_tab(kb); }
                    else tap_key(kb, KEY_UP);
                    last_move = ms;
                } else if (hy > 0) {
                    if (ms - last_tab < 200) continue;
                    last_tab = ms;
                    if (cfg_tab_mode) { fprintf(stderr, "TAB move\n"); tap_key(kb, KEY_TAB); }
                    else tap_key(kb, KEY_DOWN);
                    last_move = ms;
                } else if (hx < 0) {
                    tap_key(kb, KEY_LEFT); last_move = ms;
                } else if (hx > 0) {
                    tap_key(kb, KEY_RIGHT); last_move = ms;
                }
            }
        }

        /* hold-to-repeat only in raw-arrow mode; Tab mode is one move per press */
        if (!cfg_tab_mode && hy != 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long long ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
            if (ms - last_move >= REPEAT_MS) {
                if (hy < 0) { if (cfg_tab_mode) shift_tab(kb); else tap_key(kb, KEY_UP); }
                else        { if (cfg_tab_mode) tap_key(kb, KEY_TAB); else tap_key(kb, KEY_DOWN); }
                last_move = ms;
            }
        }
    }

    ioctl(kb, UI_DEV_DESTROY);
    close(kb);
    close(pad);
    fprintf(stderr, "dpadkeys: exit\n");
    return 0;
}
