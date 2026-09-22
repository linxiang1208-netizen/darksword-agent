/**
 * DarkSword Collector v4 - Static buffers, raw syscalls
 * NO libc, NO stack-heavy allocations
 */

/* === Inline string functions === */
static int _strlen(const char *s) { int n=0; while(s[n]) n++; return n; }
static void _memcpy(char *dst, const char *src, int n) { for(int i=0;i<n;i++) dst[i]=src[i]; }
static int _itoa(int val, char *buf) {
    int i=0; char tmp[16]; int t=0;
    if(val==0){buf[0]='0';buf[1]=0;return 1;}
    while(val>0){tmp[t++]='0'+(val%10);val/=10;}
    for(int j=t-1;j>=0;j--) buf[i++]=tmp[j];
    buf[i]=0; return i;
}

/* === Raw syscalls (no libc dependency) === */
static long _syscall3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static long _syscall2(long n, long a, long b) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory");
    return x0;
}
static long _syscall1(long n, long a) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory");
    return x0;
}

#define SYS_open 5
#define SYS_read 3
#define SYS_close 6
#define SYS_write 4

static void _write_str(const char *msg) {
    _syscall3(SYS_write, 2, (long)msg, _strlen(msg));
}

void ds_start(void) {
    _write_str("[C] ds_start entered\n");

    int fd = (int)_syscall2(SYS_open, (long)"/var/mobile/Library/SMS/sms.db", 0);
    _write_str("[C] open done\n");

    if (fd >= 0) {
        char buf[256];
        int n = (int)_syscall3(SYS_read, fd, (long)buf, 255);
        _syscall1(SYS_close, fd);

        _write_str("[C] read: ");
        char cnt[8];
        _itoa(n < 0 ? 0 : n, cnt);
        _write_str(cnt);
        _write_str(" bytes\n");
    } else {
        _write_str("[C] open failed\n");
    }

    _write_str("[C] ds_start done\n");
}