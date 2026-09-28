
#include <fcntl.h>
#include <unistd.h>

// This function is called by the exploit chain
// It reads /etc/hosts and returns first 4 bytes as proof
int _process(void *arg) {
    // Open /etc/hosts
    int fd = open("/etc/hosts", O_RDONLY);
    if (fd < 0) return -1;
    
    // Read first 4 bytes
    unsigned char buf[4] = {0};
    ssize_t n = read(fd, buf, 4);
    close(fd);
    
    if (n < 4) return -2;
    
    // Return first 4 bytes as int
    return *(int*)buf;
}
