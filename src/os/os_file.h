#pragma once

#include "base/base_arena.h"

struct FileData {
    u8* data;
    u64 size;
};

FileData os_read_file(Arena* arena, char const* path);
bool os_file_exists(char const* path);
u64 os_get_file_modified_time(char const* path);
bool os_copy_file(char const* src_path, char const* dst_path);
bool os_delete_file(char const* path);