/**
 * DarkSword Collector v52 - ZERO static variables
 * Uses return value protocol only, no persistent state
 * Each call reads /etc/hosts first 4 bytes
 */

static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory"); return x0;
}
static long _svc3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b; register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory"); return x0;
}

#define SYS_open 5
#define SYS_read 3
#define SYS_close 6

/* No static variables - just read /etc/hosts and return first 4 bytes */
int ds_start(void) {
    long fd = _svc1(SYS_open, (long)"/etc/hosts");
    if (fd < 0) return 0xFE000000;
    
    char buf[4] = {0};
    long n = _svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);
    
    if (n <= 0) return 0xEE000000;
    
    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}
