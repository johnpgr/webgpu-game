// Emscripten memory implementation - uses malloc/free (no virtual memory)
#include <stdlib.h>

#include "os/os_mod.h"

void* reserve_system_memory(u64 size) {
    // Emscripten: just malloc the full size
    void* ptr = malloc(size);
    if(ptr == nullptr) {
        LOG_FATAL("malloc failed for %llu bytes", (unsigned long long)size);
        abort();
    }
    return ptr;
}

u64 get_system_page_size(void) {
    // Emscripten: return a reasonable page size (64KB)
    return 64 * 1024;
}

void commit_system_memory(void* ptr, u64 size) {
    // Emscripten: no-op, memory already committed by malloc
    (void)ptr;
    (void)size;
}

void decommit_system_memory(void* ptr, u64 size) {
    // Emscripten: no-op, no virtual memory decommit
    (void)ptr;
    (void)size;
}

void release_system_memory(void* ptr, u64 size) {
    // Emscripten: just free
    (void)size;
    if(ptr != nullptr) {
        free(ptr);
    }
}
