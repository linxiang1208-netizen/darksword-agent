/**
 * DarkSword Collector v16 - Pure raw syscall diagnostics
 * Return value encodes diagnostic info
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

#define SYS_open  5
#define SYS_read  3
#define SYS_close 6
#define SYS_write 4

int ds_start(void) {
    int result = 0;

    /* Test 1: open /etc/hosts */
    int fd = (int)_svc2(SYS_open, (long)"/etc/hosts", 0);
    if (fd >= 0) {
        result |= 0x100; /* open ok */
        
        /* Test 2: read */
        char buf[16];
        int n = (int)_svc3(SYS_read, fd, (long)buf, 16);
        _svc1(SYS_close, fd);
        
        if (n > 0) {
            result |= 0x200; /* read ok */
            result |= (buf[0] & 0xFF); /* first byte */
        } else {
            result |= (n & 0xFF) << 16; /* read error code */
        }
    } else {
        result |= (fd & 0xFF) << 16; /* open error code */
    }

    /* Test 3: write to /tmp */
    int wfd = (int)_svc2(SYS_open, (long)"/tmp/ds_test.txt", 0x241);
    if (wfd >= 0) {
        result |= 0x400; /* write open ok */
        const char *msg = "DS";
        int wr = (int)_svc3(SYS_write, wfd, (long)msg, 2);
        _svc1(SYS_close, wfd);
        if (wr > 0) result |= 0x800; /* write ok */
    }

    return result;
}
