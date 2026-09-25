/**
 * DarkSword Collector v57 - With __DATA segments
 * Add global variables to force __DATA_CONST and __DATA segment generation
 */

#include <stdint.h>

/* Global variables to force __DATA segments */
static volatile int g_state = 0;
static volatile int g_result = 0;
static const char *g_path = "/etc/hosts";

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

void _process(void) {
    g_state = 1;
    long fd = _svc1(SYS_open, (long)g_path);
    if (fd < 0) { g_result = -1; return; }
    char buf[4] = {0};
    _svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);
    g_result = (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
    g_state = 2;
}

int _ds_start(void) {
    _process();
    return 0x56781234;
}
