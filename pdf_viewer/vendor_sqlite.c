/* SQLite deliberately stores callback variants through common function-pointer fields.
 * Keep that upstream convention and its conditional-build locals unchanged.
 * These exceptions apply only to this third-party translation unit.
 */
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wcast-function-type-mismatch")
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#endif
#if __has_warning("-Wunused-but-set-variable")
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
#endif

#include "sqlite3.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
