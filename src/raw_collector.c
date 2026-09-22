/**
 * DarkSword Collector v9 - Debug: stderr to verify syscalls work
 * NO global variables (stack only, no __DATA segment)
 */

static int _strlen(const char *s) { int n=0; while(s[n]) n++; return n; }
static int _itoa(int val, char *buf) {
    int i=0; char tmp[16]; int t=0;
    if(val==0){buf[0]='0';buf[1]=0;return 1;}
    while(val>0){tmp[t++]='0'+(val%10);val/=10;}
    for(int j=t-1;j>=0;j--) buf[i++]=tmp[j];
    buf[i]=0; return i;
}

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

static void _stderr(const char *msg) {
    _svc3(SYS_write, 2, (long)msg, _strlen(msg));
}

void ds_start(void) {
    _stderr("[v9] entered\n");

    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/SMS/sms.db", 0);
    _stderr("[v9] open fd=");
    char fb[8]; _itoa(fd, fb); _stderr(fb); _stderr("\n");

    if (fd >= 0) {
        char data[256];
        int n = (int)_svc3(SYS_read, fd, (long)data, sizeof(data)-1);
        _svc1(SYS_close, fd);
        if (n < 0) n = 0;

        _stderr("[v9] read=");
        char nb[8]; _itoa(n, nb); _stderr(nb); _stderr("\n");

        _stderr("[v9] hex=");
        for (int i = 0; i < 16 && i < n; i++) {
            char h[3]; h[0]="0123456789abcdef"[(unsigned char)data[i]>>4];
            h[1]="0123456789abcdef"[(unsigned char)data[i]&0xf]; h[2]=0;
            _stderr(h);
        }
        _stderr("\n");
    }

    int wfd = (int)_svc2(SYS_open, (long)"/tmp/ds_test.txt", 0x601);
    _stderr("[v9] wfd=");
    char wb[8]; _itoa(wfd, wb); _stderr(wb); _stderr("\n");

    if (wfd >= 0) {
        const char *t = "collector v9 was here\n";
        _svc3(SYS_write, wfd, (long)t, _strlen(t));
        _svc1(SYS_close, wfd);
        _stderr("[v9] wrote file\n");
    }

    _stderr("[v9] done\n");
}