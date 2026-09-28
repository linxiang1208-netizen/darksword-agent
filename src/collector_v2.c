// Explicit declarations - no headers needed (avoids iOS SDK path issues)
extern int open(const char *path, int oflag, ...);
extern long long read(int fd, void *buf, long long count);
extern int close(int fd);

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

// Called by Coruna exploit chain
// Reads /etc/hosts, returns first 4 bytes as proof of native file access
int _process(void *arg) {
    int fd = open("/etc/hosts", O_RDONLY);
    if (fd < 0) return -1;
    
    unsigned char buf[4] = {0};
    long long n = read(fd, buf, 4);
    close(fd);
    
    if (n < 4) return -2;
    
    return *(int*)buf;
}