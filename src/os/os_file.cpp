#include "os/os_file.h"

#pragma push_macro("internal")
#undef internal
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#pragma pop_macro("internal")

FileData os_read_file(Arena* arena, const char* path) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(path != nullptr, "Path must not be null!");

    FileData result = {};

    SDL_IOStream* file = SDL_IOFromFile(path, "rb");
    if(file == nullptr) {
        LOG_ERROR("Failed to open file '%s': %s", path, SDL_GetError());
        return result;
    }

    Sint64 file_size_i64 = SDL_GetIOSize(file);
    if(file_size_i64 < 0) {
        LOG_ERROR("Failed to get file size for '%s': %s", path, SDL_GetError());
        SDL_CloseIO(file);
        return result;
    }

    u64 file_size = (u64)file_size_i64;
    u8* buffer = nullptr;
    if(file_size > 0) {
        buffer = push_array_no_zero(arena, u8, file_size);
        usize bytes_read = SDL_ReadIO(file, buffer, (usize)file_size);
        if(bytes_read != (usize)file_size) {
            LOG_ERROR("Failed to read file '%s': %s", path, SDL_GetError());
            SDL_CloseIO(file);
            return {};
        }
    }

    SDL_CloseIO(file);
    result.data = buffer;
    result.size = file_size;
    return result;
}

bool os_file_exists(const char* path) {
    ASSERT(path != nullptr, "Path must not be null!");
    SDL_PathInfo info = {};
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

u64 os_get_file_modified_time(const char* path) {
    ASSERT(path != nullptr, "Path must not be null!");
    SDL_PathInfo info = {};
    if(!SDL_GetPathInfo(path, &info)) {
        return 0;
    }

    return (u64)info.modify_time;
}

bool os_copy_file(const char* src_path, const char* dst_path) {
    ASSERT(src_path != nullptr, "Source path must not be null!");
    ASSERT(dst_path != nullptr, "Destination path must not be null!");

    return SDL_CopyFile(src_path, dst_path);
}

bool os_delete_file(const char* path) {
    ASSERT(path != nullptr, "Path must not be null!");

    return SDL_RemovePath(path);
}
