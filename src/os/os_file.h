#pragma once

#include "base/base_arena.h"

struct FileData {
    u8* data;
    u64 size;
};

FileData os_read_file(Arena* arena, const char* path);
bool os_file_exists(const char* path);
u64 os_get_file_modified_time(const char* path);
bool os_copy_file(const char* src_path, const char* dst_path);
bool os_delete_file(const char* path);
