/**
 * Compatibility layer for running ClearSword as a dylib
 * Provides stubs for functions that don't work in dylib context
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Stub for _NSGetExecutablePath - returns a fake path */
int _NSGetExecutablePath(char *path, uint32_t *size) {
    const char *fake_path = "/usr/sbin/Safari";
    uint32_t needed = strlen(fake_path) + 1;
    if (*size < needed) {
        *size = needed;
        return -1;
    }
    strcpy(path, fake_path);
    return 0;
}

/* Provide vm_page_size if not available */
/* Note: This should be available from the kernel, but let's provide a fallback */
/* extern vm_size_t vm_page_size; */ /* Should be available */
