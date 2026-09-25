/* Ultra-minimal test: just return a marker, no dependencies */
int ds_start(void) {
    return 0x01000001; /* marker: dylib loaded successfully */
}
