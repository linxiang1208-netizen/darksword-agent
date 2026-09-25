/**
 * DarkSword Collector v54 - Uses standard C library
 * Compiled WITHOUT -nostdlib to match original bootstrap flags
 * Relies on MachOPayloadBuilder to resolve symbols
 */

#include <fcntl.h>
#include <unistd.h>

/* Entry point - Stage3 calls _process */
void _process(void) {
    /* Read /etc/hosts first 4 bytes */
    int fd = open("/etc/hosts", O_RDONLY);
    if (fd < 0) return;
    char buf[4] = {0};
    read(fd, buf, 4);
    close(fd);
}

/* Also export _ds_start for compatibility */
int _ds_start(void) {
    _process();
    return 0x56781234; /* marker: dylib executed successfully */
}
