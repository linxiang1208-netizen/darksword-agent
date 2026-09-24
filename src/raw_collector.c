/**
 * DarkSword Collector v30 - Use static volatile for offset persistence
 * ds_start is called multiple times on the SAME mapped memory
 * static volatile int should persist between calls
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
#define O_RDONLY   0

static volatile int g_offset = 0;

int ds_start(void) {
    int offset = g_offset;

    int fd = (int)_svc2(SYS_open, (long)"/etc/hosts", O_RDONLY);
    if (fd < 0) { g_offset = 0; return 0xDEAD0000; }

    if (offset > 0) _svc4(SYS_lseek, fd, (long)offset, SEEK_SET, 0);

    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);

    if (n < 1) { g_offset = 0; return 0xFFFFFFFF; }

    g_offset = offset + n;

    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
