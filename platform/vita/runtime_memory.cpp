/* VitaSDK consumes these unmangled symbols before entering main().
 * Do not reserve the default 128 MiB heap on top of an artwork-sized SELF.
 * Backdrops live in app0:; leave room for GXM/display/audio allocations too.
 */
extern "C" {
unsigned int _newlib_heap_size_user = 96U * 1024U * 1024U;

/* The shared runtime keeps simulation/PPU snapshots on the main thread.
 * Its desktop entry frame alone exceeds 256 KiB, before nested calls.
 */
extern const unsigned int sceUserMainThreadStackSize = 2U * 1024U * 1024U;
}
