// mousemode.c — gamepad -> pointer (uinput) with a configurable toggle chord
//
// Target: Anbernic RG405V / GammaOS Next (Android 14), portable to any
// Linux/Android box with evdev + uinput.
//
// Defaults:
//   source device : auto-detect ("Xbox Wireless Controller" / gammapad virtual)
//   toggle chord  : L1 + R1 + Select
//   pointer       : right stick (ABS_Z / ABS_RZ)
//   scroll        : left stick  (ABS_Y -> vertical, ABS_X -> horizontal)
//   clicks        : A = left, B = right, X = middle
//
// While mouse mode is on the source device is EVIOCGRAB'd, so games/frontends
// stop receiving pad input until the chord is pressed again.
//
// Build (static, aarch64):
//   aarch64-linux-gnu-gcc -O2 -Wall -static -o mousemode mousemode.c
//
// Run:
//   ./mousemode [-d /dev/input/eventN] [-s px_per_sec] [--dz n] [-v]

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define NBITS(x)      ((((x) - 1) / BITS_PER_LONG) + 1)
#define TEST_BIT(bit, array) \
    ((array)[(bit) / BITS_PER_LONG] & (1UL << ((bit) % BITS_PER_LONG)))

#define TICK_MS 8

static volatile sig_atomic_t running = 1;
static int verbose = 0;

/* ---- config ---- */
static int   cfg_deadzone    = 300;    /* of 1800 */
static float cfg_speed       = 700.0f; /* px per second at full deflection */
static float cfg_scroll_rate = 9.0f;   /* wheel notches per second */
static int   cfg_invert_y    = 0;
static int   cfg_ab_left     = 1;  /* 1: A-label (east) = left click */

/* ---- state ---- */
static int mouse_on = 0;
static int src_fd = -1;
static int uin_fd = -1;

static int has_rx_ry = 0;              /* pad uses ABS_RX/RY instead of Z/RZ */
static int val_x, val_y, val_z, val_rz;

static float acc_x = 0.0f, acc_y = 0.0f;
static float scroll_v = 0.0f, scroll_h = 0.0f;

static int tl_down, tr_down, sel_down, chord_latched;

static int max_x = 1800, max_y = 1800, max_z = 1800, max_rz = 1800;

static void on_signal(int sig) { (void)sig; running = 0; }

static void logmsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ---------------- uinput ---------------- */

static int create_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open /dev/uinput"); return -1; }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor  = 0x1209;
    usetup.id.product = 0x0001;
    usetup.id.version = 1;
    snprintf(usetup.name, sizeof(usetup.name), "andr36oid-mouse");

    if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0) { perror("UI_DEV_SETUP"); close(fd); return -1; }
    if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("UI_DEV_CREATE"); close(fd); return -1; }
    return fd;
}

static void emit(int fd, int type, int code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type  = type;
    ev.code  = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) < 0 && errno != EAGAIN)
        perror("uinput write");
}

static void emit_syn(int fd) { emit(fd, EV_SYN, SYN_REPORT, 0); }

/* ---------------- device discovery ---------------- */

static const char *device_name(int fd) {
    static char name[256];
    memset(name, 0, sizeof(name));
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) return "";
    return name;
}

/*
 * Score a device as a mouse-mode source. We need REAL sticks and the chord
 * buttons; a name match alone is not enough (GammaOS exposes a "GammaPad
 * Virtual Mouse" which has no axes at all).
 */
static int score_device(int fd) {
    const char *n = device_name(fd);
    if (!n[0]) return 0;
    if (strcasestr(n, "mouse")) return 0;      /* never the pointer device */

    unsigned long absbits[NBITS(ABS_MAX)] = {0};
    unsigned long keybits[NBITS(KEY_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) < 0) return 0;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) < 0) return 0;

    int has_left  = TEST_BIT(ABS_X, absbits) && TEST_BIT(ABS_Y, absbits);
    int has_right = (TEST_BIT(ABS_Z, absbits) && TEST_BIT(ABS_RZ, absbits)) ||
                    (TEST_BIT(ABS_RX, absbits) && TEST_BIT(ABS_RY, absbits));
    if (!has_left || !has_right) return 0;

    int score = 1;
    if (TEST_BIT(BTN_SELECT, keybits)) score += 2;
    if (TEST_BIT(BTN_TL, keybits))     score += 1;
    if (TEST_BIT(BTN_TR, keybits))     score += 1;
    if (TEST_BIT(BTN_SOUTH, keybits) && TEST_BIT(BTN_EAST, keybits)) score += 1;

    if (strcasestr(n, "Xbox Wireless Controller")) score += 4;
    if (strcasestr(n, "GammaPad"))                 score += 3;
    if (strcasestr(n, "joypad"))                   score += 3;
    return score;
}

static int find_device(char *out, size_t outlen) {
    DIR *d = opendir("/dev/input");
    if (!d) { perror("opendir /dev/input"); return -1; }
    struct dirent *de;
    int best = 0;
    while ((de = readdir(d))) {
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        char path[300];
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        int s = score_device(fd);
        if (verbose && s) logmsg("candidate: %s (%s) score=%d", path, device_name(fd), s);
        if (s > best) {
            best = s;
            snprintf(out, outlen, "%s", path);
        }
        close(fd);
    }
    closedir(d);
    return best > 0 ? 0 : -1;
}

/* ---------------- axis helpers ---------------- */

static int read_abs_max(int fd, int axis, int fallback) {
    struct input_absinfo ai;
    if (ioctl(fd, EVIOCGABS(axis), &ai) == 0 && ai.maximum > ai.minimum)
        return ai.maximum;
    return fallback;
}

static void query_caps(int fd) {
    unsigned long absbits[NBITS(ABS_MAX)] = {0};

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) < 0)
        perror("EVIOCGBIT abs");

    int hx  = TEST_BIT(ABS_X, absbits);
    int hy  = TEST_BIT(ABS_Y, absbits);
    int hz  = TEST_BIT(ABS_Z, absbits);
    int hrz = TEST_BIT(ABS_RZ, absbits);

    if (!hz && TEST_BIT(ABS_RX, absbits))  { has_rx_ry = 1; hz = 1; }
    if (!hrz && TEST_BIT(ABS_RY, absbits)) { has_rx_ry = 1; hrz = 1; }

    int zaxis  = has_rx_ry ? ABS_RX : ABS_Z;
    int rzaxis = has_rx_ry ? ABS_RY : ABS_RZ;

    max_x  = read_abs_max(fd, ABS_X, 1800);
    max_y  = read_abs_max(fd, ABS_Y, 1800);
    max_z  = read_abs_max(fd, zaxis, 1800);
    max_rz = read_abs_max(fd, rzaxis, 1800);

    if (verbose)
        logmsg("axes: X=%d(max %d) Y=%d(max %d) right=%s Z=%d(max %d) RZ=%d(max %d)",
               hx, max_x, hy, max_y,
               has_rx_ry ? "RX/RY" : "Z/RZ", hz, max_z, hrz, max_rz);
}

static float norm(int value, int max) {
    float v = (float)value / (float)(max > 0 ? max : 1);
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
}

static float apply_deadzone(float v, int dz, int max) {
    float d = (float)dz / (float)(max > 0 ? max : 1);
    float a = v < 0 ? -v : v;
    if (a <= d) return 0.0f;
    float s = (a - d) / (1.0f - d);
    if (s > 1.0f) s = 1.0f;
    return v < 0 ? -s : s;
}

/* ---------------- mouse mode ---------------- */

static void set_mouse_mode(int on) {
    if (on == mouse_on) return;
    mouse_on = on;
    acc_x = acc_y = scroll_v = scroll_h = 0.0f;

    int rc = ioctl(src_fd, EVIOCGRAB, on ? 1 : 0);
    logmsg("mouse mode %s (grab %s)", on ? "ON" : "OFF",
           rc == 0 ? "ok" : strerror(errno));

    if (!on) {
        emit(uin_fd, EV_KEY, BTN_LEFT, 0);
        emit(uin_fd, EV_KEY, BTN_RIGHT, 0);
        emit(uin_fd, EV_KEY, BTN_MIDDLE, 0);
        emit_syn(uin_fd);
    }
}

static void mouse_tick(void) {
    float dx = apply_deadzone(norm(val_z,  max_z),  cfg_deadzone, max_z);
    float dy = apply_deadzone(norm(val_rz, max_rz), cfg_deadzone, max_rz);
    float sx = apply_deadzone(norm(val_x,  max_x),  cfg_deadzone, max_x);
    float sy = apply_deadzone(norm(val_y,  max_y),  cfg_deadzone, max_y);

    const float dt = TICK_MS / 1000.0f;

    /* quadratic response: fine near centre, fast at the edge */
    acc_x += dx * (dx < 0 ? -dx : dx) * cfg_speed * dt;
    acc_y += dy * (dy < 0 ? -dy : dy) * cfg_speed * dt;
    if (cfg_invert_y) acc_y = -acc_y;

    int mx = (int)acc_x;
    int my = (int)acc_y;
    if (mx) { emit(uin_fd, EV_REL, REL_X, mx); acc_x -= mx; }
    if (my) { emit(uin_fd, EV_REL, REL_Y, my); acc_y -= my; }

    /* left stick -> scroll */
    scroll_v += (-sy) * (sy < 0 ? -sy : sy) * cfg_scroll_rate * dt;
    scroll_h += ( sx) * (sx < 0 ? -sx : sx) * cfg_scroll_rate * dt;

    int nv = (int)scroll_v;
    int nh = (int)scroll_h;
    if (nv) { emit(uin_fd, EV_REL, REL_WHEEL,  nv); scroll_v -= nv; }
    if (nh) { emit(uin_fd, EV_REL, REL_HWHEEL, nh); scroll_h -= nh; }

    emit_syn(uin_fd);
}

/* ---------------- main ---------------- */


/* Optional config: /data/local/tmp/mousemode.conf
 *   speed=700
 *   deadzone=300
 *   invert_y=0
 */
static void load_config(void) {
    FILE *f = fopen("/data/local/tmp/mousemode.conf", "r");
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
        if (!strcmp(k, "speed"))         cfg_speed = (float)atof(v);
        else if (!strcmp(k, "deadzone")) cfg_deadzone = atoi(v);
        else if (!strcmp(k, "invert_y")) cfg_invert_y = atoi(v);
        else if (!strcmp(k, "ab_left"))  cfg_ab_left = atoi(v);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    char dev_path[256] = {0};

    load_config();

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) snprintf(dev_path, sizeof(dev_path), "%s", argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) cfg_speed = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--dz") && i + 1 < argc) cfg_deadzone = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: %s [-d /dev/input/eventN] [-s speed] [--dz n] [-v]\n", argv[0]);
            return 2;
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (!dev_path[0]) {
        if (find_device(dev_path, sizeof(dev_path)) < 0) {
            fprintf(stderr, "no gamepad found (Xbox Wireless Controller / GammaPad / joypad)\n");
            return 1;
        }
    }

    src_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (src_fd < 0) { perror(dev_path); return 1; }
    query_caps(src_fd);
    logmsg("source: %s (%s)", dev_path, device_name(src_fd));
    logmsg("config: speed=%.0f deadzone=%d invert_y=%d ab_left=%d", cfg_speed, cfg_deadzone, cfg_invert_y, cfg_ab_left);

    uin_fd = create_uinput();
    if (uin_fd < 0) return 1;
    logmsg("virtual pointer created");

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(src_fd, &fds);
        struct timeval tv = {0, TICK_MS * 1000};

        int r = select(src_fd + 1, &fds, NULL, NULL, &tv);
        if (r < 0 && errno != EINTR) { perror("select"); break; }

        if (r > 0 && FD_ISSET(src_fd, &fds)) {
            struct input_event ev;
            while (read(src_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type == EV_ABS) {
                    switch (ev.code) {
                    case ABS_X:  val_x  = ev.value; break;
                    case ABS_Y:  val_y  = ev.value; break;
                    case ABS_Z:  val_z  = ev.value; break;
                    case ABS_RZ: val_rz = ev.value; break;
                    case ABS_RX: if (has_rx_ry) val_z  = ev.value; break;
                    case ABS_RY: if (has_rx_ry) val_rz = ev.value; break;
                    }
                } else if (ev.type == EV_KEY) {
                    int down = ev.value != 0;
                    switch (ev.code) {
                    case BTN_TL:     tl_down  = down; break;
                    case BTN_TR:     tr_down  = down; break;
                    case BTN_SELECT: sel_down = down; break;
                    }

                    /* chord: L1 + R1 + Select */
                    if (tl_down && tr_down && sel_down) {
                        if (!chord_latched) { chord_latched = 1; set_mouse_mode(!mouse_on); }
                    } else {
                        chord_latched = 0;
                    }

                    if (mouse_on) {
                        int mbtn = 0;
                        switch (ev.code) {
                        /* Nintendo layout: the button labelled "A" is the east
                         * face button. ab_left=1 keeps A = left click. */
                        case BTN_SOUTH: mbtn = cfg_ab_left ? BTN_RIGHT : BTN_LEFT; break;
                        case BTN_EAST:  mbtn = cfg_ab_left ? BTN_LEFT  : BTN_RIGHT; break;
                        case BTN_WEST:  mbtn = BTN_MIDDLE; break;
                        case BTN_NORTH: mbtn = BTN_MIDDLE; break;
                        }
                        if (mbtn) { emit(uin_fd, EV_KEY, mbtn, down); emit_syn(uin_fd); }
                    }
                }
            }
        }

        if (mouse_on) mouse_tick();
    }

    if (mouse_on) set_mouse_mode(0);
    if (uin_fd >= 0) {
        ioctl(uin_fd, UI_DEV_DESTROY);
        close(uin_fd);
    }
    if (src_fd >= 0) close(src_fd);
    logmsg("exit");
    return 0;
}
