/**
 * Collector dylib wrapper for MachOPayloadBuilder loading
 * Includes raw_collector and adds constructor entry point
 */

/* Forward declare the raw collector entry */
extern void ds_collect(void);

/* Constructor - runs on dlopen by MachOPayloadBuilder */
__attribute__((constructor))
void _process(void) {
    ds_collect();
}