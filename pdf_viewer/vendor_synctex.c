/* SyncTeX retains labels, counters, and compatibility helpers used by other builds.
 * These exceptions apply only to this third-party translation unit.
 */
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunused-label")
#pragma clang diagnostic ignored "-Wunused-label"
#endif
#if __has_warning("-Wunused-but-set-variable")
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
#if __has_warning("-Wunneeded-internal-declaration")
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#endif
#endif

#include "synctex/synctex_parser.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
