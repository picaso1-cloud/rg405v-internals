// hidpad.c — feed a USB HID gamepad gadget (/dev/hidg0) from evdev
//
// Report layout (6 bytes, matches hid_desc.bin):
//   [0] X   [1] Y   [2] Z   [3] Rz   (0..255, 128 = centre)
//   [4] buttons 0-7   [5] buttons 8-15
//
// Button map (bit):
//   0 A   1 B   2 X   3 Y
//   4 L1  5 R1  6 L2  7 R2
//   8 Select  9 Start  10 L3  11 R3
//   12 D-up  13 D-down  14 D-left  15 D-right
//
// Build: aarch64-linux-gnu-gcc -O2 -static -o hidpad hidpad.c

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
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

static volatile sig_atomic_t running = 1;
static void on_signal(int s) { (void)s; running = 0; }

static const char *devname(int fd) {
    static char n[256];
    memset(n, 0, sizeof(n));
    ioctl(fd, EVIOCGNAME(sizeof(n) - 1), n);
    return n;
}

static int score_device(int fd) {
    const char *n = devname(fd);
    if (!n[0] || strcasestr(n, "mouse")) return 0;
    unsigned long absb[NBITS(ABS_MAX)] = {0}, keyb[NBITS(KEY_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absb)), absb) < 0) return 0;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyb)), keyb) < 0) return 0;
    if (!(TEST_BIT(ABS_X, absb) && TEST_BIT(ABS_Y, absb))) return 0;
    if (!((TEST_BIT(ABS_Z, absb) && TEST_BIT(ABS_RZ, absb)) ||
          (TEST_BIT(ABS_RX, absb) && TEST_BIT(ABS_RY, absb)))) return 0;
    int s = 1;
    if (TEST_BIT(BTN_SOUTH, keyb) && TEST_BIT(BTN_EAST, keyb)) s += 2;
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
        int s = score_device(fd);
        if (s > best) { best = s; snprintf(out, len, "%s", p); }
        close(fd);
    }
    closedir(d);
    return best > 0 ? 0 : -1;
}

static int absmax(int fd, int axis) {
    struct input_absinfo ai;
    if (ioctl(fd, EVIOCGABS(axis), &ai) == 0 && ai.maximum > ai.minimum)
        return ai.maximum;
    return 1800;
}

/* -1800..1800 (or 0..1800 for triggers) -> 0..255 */
static uint8_t axis8(int v, int mx) {
    if (mx <= 0) mx = 1800;
    long r = ((long)v + mx) * 255 / (2L * mx);
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    return (uint8_t)r;
}

int main(int argc, char **argv) {
    char path[256] = {0};
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-d") && i + 1 < argc) snprintf(path, sizeof(path), "%s", argv[++i]);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (!path[0] && find_pad(path, sizeof(path)) < 0) {
        fprintf(stderr, "no gamepad found\n");
        return 1;
    }

    int pad = open(path, O_RDONLY | O_NONBLOCK);
    if (pad < 0) { perror(path); return 1; }
    fprintf(stderr, "hidpad: source %s (%s)\n", path, devname(pad));

    int mx = absmax(pad, ABS_X), my = absmax(pad, ABS_Y);
    int mz = absmax(pad, ABS_Z), mrz = absmax(pad, ABS_RZ);

    int hid = -1;
    for (int tries = 0; tries < 100 && hid < 0; tries++) {
        hid = open("/dev/hidg0", O_RDWR | O_NONBLOCK);
        if (hid < 0) usleep(100000);
    }
    if (hid < 0) { perror("/dev/hidg0"); close(pad); return 1; }
    fprintf(stderr, "hidpad: /dev/hidg0 open\n");

    int vx = 0, vy = 0, vz = 0, vrz = 0, hx = 0, hy = 0;
    uint16_t buttons = 0;
    uint8_t report[6] = {128, 128, 128, 128, 0, 0};
    uint8_t last[6] = {0};

    time_t last_send = 0;
    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(pad, &fds);
        struct timeval tv = {0, 100 * 1000};
        int r = select(pad + 1, &fds, NULL, NULL, &tv);
        if (r < 0 && errno != EINTR) break;

        if (r > 0 && FD_ISSET(pad, &fds)) {
            struct input_event ev;
            while (read(pad, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type == EV_ABS) {
                    switch (ev.code) {
                    case ABS_X:  vx = ev.value; break;
                    case ABS_Y:  vy = ev.value; break;
                    case ABS_Z:  vz = ev.value; break;
                    case ABS_RZ: vrz = ev.value; break;
                    case ABS_RX: vz = ev.value; break;
                    case ABS_RY: vrz = ev.value; break;
                    case ABS_HAT0X: hx = ev.value; break;
                    case ABS_HAT0Y: hy = ev.value; break;
                    }
                } else if (ev.type == EV_KEY) {
                    int bit = -1;
                    switch (ev.code) {
                    case BTN_SOUTH:  bit = 0;  break;
                    case BTN_EAST:   bit = 1;  break;
                    case BTN_WEST:   bit = 2;  break;
                    case BTN_NORTH:  bit = 3;  break;
                    case BTN_TL:     bit = 4;  break;
                    case BTN_TR:     bit = 5;  break;
                    case BTN_TL2:    bit = 6;  break;
                    case BTN_TR2:    bit = 7;  break;
                    case BTN_SELECT: bit = 8;  break;
                    case BTN_START:  bit = 9;  break;
                    case BTN_THUMBL: bit = 10; break;
                    case BTN_THUMBR: bit = 11; break;
                    }
                    if (bit >= 0) {
                        if (ev.value) buttons |= (uint16_t)(1u << bit);
                        else          buttons &= (uint16_t)~(1u << bit);
                    }
                }
            }
        }

        uint16_t dpad = buttons & 0xF000;
        buttons &= 0x0FFF;
        if (hy < 0) dpad |= 0x1000; else dpad &= ~0x1000;
        if (hy > 0) dpad |= 0x2000; else dpad &= ~0x2000;
        if (hx < 0) dpad |= 0x4000; else dpad &= ~0x4000;
        if (hx > 0) dpad |= 0x8000; else dpad &= ~0x8000;
        buttons |= dpad;

        report[0] = axis8(vx, mx);
        report[1] = axis8(vy, my);
        report[2] = axis8(vz, mz);
        report[3] = axis8(vrz, mrz);
        report[4] = (uint8_t)(buttons & 0xFF);
        report[5] = (uint8_t)(buttons >> 8);

        time_t now = time(NULL);
        if (memcmp(report, last, sizeof(report)) != 0 || now != last_send) {
            ssize_t w = write(hid, report, sizeof(report));
            if (w < 0) {
                if (errno == ENOENT || errno == ESHUTDOWN || errno == EPIPE) {
                    fprintf(stderr, "hidpad: gadget gone (%s)\n", strerror(errno));
                    break;
                }
            } else {
                memcpy(last, report, sizeof(report));
                last_send = now;
            }
        }
    }

    close(hid);
    close(pad);
    fprintf(stderr, "hidpad: exit\n");
    return 0;
}
