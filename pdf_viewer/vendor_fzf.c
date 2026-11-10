/* fzf shares a callback signature across scorers and uses signed scoring indices.
 * These exceptions apply only to this third-party translation unit.
 */
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunused-parameter")
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#if __has_warning("-Wsign-compare")
#pragma clang diagnostic ignored "-Wsign-compare"
#endif
#endif

#include "../fzf/fzf.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
