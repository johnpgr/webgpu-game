#include <stdio.h>
#include <sys/stat.h>

#include "os/os_file.h"

#if OS_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

FileData os_read_file(Arena* arena, const char* path) {
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

bool os_file_exists(const char* path) {
    ASSERT(path != nullptr, "Path must not be null!");
#if OS_WINDOWS
    DWORD attribs = GetFileAttributesA(path);
    return attribs != INVALID_FILE_ATTRIBUTES &&
           !(attribs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

u64 os_get_file_modified_time(const char* path) {
    ASSERT(path != nullptr, "Path must not be null!");
#if OS_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA data;
    if(!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return 0;
    }
    ULARGE_INTEGER ul;
    ul.HighPart = data.ftLastWriteTime.dwHighDateTime;
    ul.LowPart = data.ftLastWriteTime.dwLowDateTime;
    return ul.QuadPart;
#else
    struct stat st;
    if(stat(path, &st) != 0) {
        return 0;
    }
    return (u64)st.st_mtime;
#endif
}

bool os_copy_file(const char* src_path, const char* dst_path) {
    ASSERT(src_path != nullptr, "Source path must not be null!");
    ASSERT(dst_path != nullptr, "Destination path must not be null!");

#if OS_WINDOWS
    return CopyFileA(src_path, dst_path, FALSE) != 0;
#else
    FILE* src = fopen(src_path, "rb");
    if(!src) {
        LOG_ERROR("Failed to open source file for copy: %s", src_path);
        return false;
    }

    FILE* dst = fopen(dst_path, "wb");
    if(!dst) {
        LOG_ERROR("Failed to open destination file for copy: %s", dst_path);
        fclose(src);
        return false;
    }

    char buffer[4096];
    size_t bytes_read;
    bool success = true;

    while((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if(fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
            LOG_ERROR("Failed to write to destination file: %s", dst_path);
            success = false;
            break;
        }
    }

    fclose(src);
    fclose(dst);
    return success;
#endif
}

bool os_delete_file(const char* path) {
    ASSERT(path != nullptr, "Path must not be null!");
#if OS_WINDOWS
    return DeleteFileA(path) != 0;
#else
    return unlink(path) == 0;
#endif
}
