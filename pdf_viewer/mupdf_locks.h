#pragma once

#include <mupdf/fitz.h>
#include <mutex>

// Every MuPDF context in the process must use these same locks, even contexts
// with independent stores. In particular, load-jpx.c protects OpenJPEG's global
// allocator context with FZ_LOCK_FREETYPE; per-worker/no-op locks are unsafe.
inline fz_locks_context* shared_mupdf_locks() {
    static std::mutex mutexes[FZ_LOCK_MAX];
    static fz_locks_context locks{
        mutexes,
        [](void* user, int index) { static_cast<std::mutex*>(user)[index].lock(); },
        [](void* user, int index) { static_cast<std::mutex*>(user)[index].unlock(); }
    };
    return &locks;
}
