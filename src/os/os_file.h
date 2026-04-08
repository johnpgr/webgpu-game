#pragma once

#include "base/base_arena.h"

struct FileData {
    u8* data;
    u64 size;
};

FileData os_read_file(Arena* arena, char const* path);