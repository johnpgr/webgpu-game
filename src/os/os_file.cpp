#include <stdio.h>

#include "os/os_file.h"

FileData os_read_file(Arena* arena, char const* path) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(path != nullptr, "Path must not be null!");

    FileData result = {};

    FILE* file = fopen(path, "rb");
    if(file == nullptr) {
        LOG_ERROR("Failed to open file: %s", path);
        return result;
    }

    if(fseek(file, 0, SEEK_END) != 0) {
        LOG_ERROR("Failed to seek to file end: %s", path);
        fclose(file);
        return result;
    }

    long file_size_long = ftell(file);
    if(file_size_long < 0) {
        LOG_ERROR("Failed to get file size: %s", path);
        fclose(file);
        return result;
    }

    if(fseek(file, 0, SEEK_SET) != 0) {
        LOG_ERROR("Failed to seek to file start: %s", path);
        fclose(file);
        return result;
    }

    u64 file_size = (u64)file_size_long;
    u8* buffer = nullptr;
    if(file_size > 0) {
        buffer = push_array_no_zero(arena, u8, file_size);
        usize bytes_read = fread(buffer, 1, (usize)file_size, file);
        if(bytes_read != (usize)file_size) {
            LOG_ERROR("Failed to read file: %s", path);
            fclose(file);
            return {};
        }
    }

    fclose(file);
    result.data = buffer;
    result.size = file_size;
    return result;
}
