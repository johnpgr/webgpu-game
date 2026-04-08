#include <stdlib.h>
#include <string.h>

#pragma push_macro("internal")
#undef internal
#include <SDL3/SDL.h>
#if OS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#include <SDL3_image/SDL_image.h>
#endif
#pragma pop_macro("internal")

#include "assets/assets_atlas.h"
#include "os/os_mod.h"

internal bool json_is_whitespace(u8 c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

internal void skip_whitespace(u8* data, u64 size, u64* pos) {
    while(*pos < size && json_is_whitespace(data[*pos])) {
        ++(*pos);
    }
}

internal bool consume_char(u8* data, u64 size, u64* pos, char c) {
    skip_whitespace(data, size, pos);
    if(*pos < size && data[*pos] == (u8)c) {
        ++(*pos);
        return true;
    }
    return false;
}

internal bool expect_char(u8* data, u64 size, u64* pos, char c) {
    if(consume_char(data, size, pos, c)) {
        return true;
    }

    LOG_ERROR("Malformed atlas JSON: expected '%c'", c);
    return false;
}

internal String parse_json_string(u8* data, u64 size, u64* pos) {
    skip_whitespace(data, size, pos);
    if(*pos >= size || data[*pos] != '"') {
        LOG_ERROR("Malformed atlas JSON: expected string");
        return {};
    }

    ++(*pos);
    u64 start = *pos;
    while(*pos < size) {
        u8 c = data[*pos];
        if(c == '\\') {
            *pos += 2;
            continue;
        }
        if(c == '"') {
            String result = {data + start, *pos - start};
            ++(*pos);
            return result;
        }
        ++(*pos);
    }

    LOG_ERROR("Malformed atlas JSON: unterminated string");
    return {};
}

internal u64 scan_json_number(u8* data, u64 size, u64 pos) {
    u64 start = pos;
    if(pos < size && (data[pos] == '-' || data[pos] == '+')) {
        ++pos;
    }
    while(pos < size && data[pos] >= '0' && data[pos] <= '9') {
        ++pos;
    }
    if(pos < size && data[pos] == '.') {
        ++pos;
        while(pos < size && data[pos] >= '0' && data[pos] <= '9') {
            ++pos;
        }
    }
    if(pos < size && (data[pos] == 'e' || data[pos] == 'E')) {
        ++pos;
        if(pos < size && (data[pos] == '-' || data[pos] == '+')) {
            ++pos;
        }
        while(pos < size && data[pos] >= '0' && data[pos] <= '9') {
            ++pos;
        }
    }
    return pos - start;
}

internal f32 parse_json_number_f32(u8* data, u64 size, u64* pos) {
    skip_whitespace(data, size, pos);
    u64 len = scan_json_number(data, size, *pos);
    if(len == 0 || len >= 64) {
        LOG_ERROR("Malformed atlas JSON: invalid float");
        return 0.0f;
    }

    char buffer[64] = {};
    memcpy(buffer, data + *pos, (usize)len);
    *pos += len;
    return strtof(buffer, nullptr);
}

internal u32 parse_json_number_u32(u8* data, u64 size, u64* pos) {
    skip_whitespace(data, size, pos);
    u64 len = scan_json_number(data, size, *pos);
    if(len == 0 || len >= 32) {
        LOG_ERROR("Malformed atlas JSON: invalid integer");
        return 0;
    }

    char buffer[32] = {};
    memcpy(buffer, data + *pos, (usize)len);
    *pos += len;
    return (u32)strtoul(buffer, nullptr, 10);
}

internal void skip_json_value(u8* data, u64 size, u64* pos);

internal void skip_json_array(u8* data, u64 size, u64* pos) {
    if(!expect_char(data, size, pos, '[')) {
        return;
    }

    if(consume_char(data, size, pos, ']')) {
        return;
    }

    for(;;) {
        skip_json_value(data, size, pos);
        if(consume_char(data, size, pos, ',')) {
            continue;
        }
        expect_char(data, size, pos, ']');
        return;
    }
}

internal void skip_json_object(u8* data, u64 size, u64* pos) {
    if(!expect_char(data, size, pos, '{')) {
        return;
    }

    if(consume_char(data, size, pos, '}')) {
        return;
    }

    for(;;) {
        parse_json_string(data, size, pos);
        expect_char(data, size, pos, ':');
        skip_json_value(data, size, pos);
        if(consume_char(data, size, pos, ',')) {
            continue;
        }
        expect_char(data, size, pos, '}');
        return;
    }
}

internal void skip_json_value(u8* data, u64 size, u64* pos) {
    skip_whitespace(data, size, pos);
    if(*pos >= size) {
        return;
    }

    u8 c = data[*pos];
    if(c == '{') {
        skip_json_object(data, size, pos);
    } else if(c == '[') {
        skip_json_array(data, size, pos);
    } else if(c == '"') {
        parse_json_string(data, size, pos);
    } else {
        u64 len = scan_json_number(data, size, *pos);
        if(len > 0) {
            *pos += len;
            return;
        }

        if(*pos + 4 <= size && memcmp(data + *pos, "true", 4) == 0) {
            *pos += 4;
        } else if(*pos + 5 <= size && memcmp(data + *pos, "false", 5) == 0) {
            *pos += 5;
        } else if(*pos + 4 <= size && memcmp(data + *pos, "null", 4) == 0) {
            *pos += 4;
        } else {
            LOG_ERROR("Malformed atlas JSON: unsupported value");
        }
    }
}

internal bool string_equals_cstr(String a, char const* b) {
    return string_equals(a, string_lit(b));
}

internal u32 count_token_occurrences(u8* data, u64 size, char const* token) {
    u64 token_size = (u64)strlen(token);
    u32 count = 0;
    if(token_size == 0 || token_size > size) {
        return 0;
    }

    for(u64 i = 0; i + token_size <= size; ++i) {
        if(memcmp(data + i, token, (usize)token_size) == 0) {
            ++count;
        }
    }
    return count;
}

internal void parse_atlas_metadata(Atlas* atlas, u8* data, u64 size, u64* pos) {
    if(!expect_char(data, size, pos, '{')) {
        return;
    }

    if(consume_char(data, size, pos, '}')) {
        return;
    }

    for(;;) {
        String key = parse_json_string(data, size, pos);
        expect_char(data, size, pos, ':');

        if(string_equals_cstr(key, "width")) {
            atlas->atlas_width = parse_json_number_u32(data, size, pos);
        } else if(string_equals_cstr(key, "height")) {
            atlas->atlas_height = parse_json_number_u32(data, size, pos);
        } else {
            skip_json_value(data, size, pos);
        }

        if(consume_char(data, size, pos, ',')) {
            continue;
        }
        expect_char(data, size, pos, '}');
        return;
    }
}

internal void parse_frame_object(
    AtlasFrame* frame,
    u8* data,
    u64 size,
    u64* pos
) {
    if(!expect_char(data, size, pos, '{')) {
        return;
    }

    if(consume_char(data, size, pos, '}')) {
        return;
    }

    for(;;) {
        String key = parse_json_string(data, size, pos);
        expect_char(data, size, pos, ':');

        if(string_equals_cstr(key, "u0")) {
            frame->uv_min.x = parse_json_number_f32(data, size, pos);
        } else if(string_equals_cstr(key, "v0")) {
            frame->uv_min.y = parse_json_number_f32(data, size, pos);
        } else if(string_equals_cstr(key, "u1")) {
            frame->uv_max.x = parse_json_number_f32(data, size, pos);
        } else if(string_equals_cstr(key, "v1")) {
            frame->uv_max.y = parse_json_number_f32(data, size, pos);
        } else if(string_equals_cstr(key, "w")) {
            frame->width_px = (f32)parse_json_number_u32(data, size, pos);
        } else if(string_equals_cstr(key, "h")) {
            frame->height_px = (f32)parse_json_number_u32(data, size, pos);
        } else {
            (void)parse_json_number_f32(data, size, pos);
        }

        if(consume_char(data, size, pos, ',')) {
            continue;
        }
        expect_char(data, size, pos, '}');
        return;
    }
}

internal void parse_sprite_object(
    AtlasSprite* sprite,
    AtlasFrame* frames,
    u32* frame_index,
    u8* data,
    u64 size,
    u64* pos
) {
    if(!expect_char(data, size, pos, '{')) {
        return;
    }

    if(consume_char(data, size, pos, '}')) {
        return;
    }

    for(;;) {
        String key = parse_json_string(data, size, pos);
        expect_char(data, size, pos, ':');

        if(string_equals_cstr(key, "frames")) {
            if(!expect_char(data, size, pos, '[')) {
                return;
            }

            sprite->frames = frames + *frame_index;
            sprite->frame_count = 0;

            if(!consume_char(data, size, pos, ']')) {
                for(;;) {
                    AtlasFrame* frame = frames + *frame_index;
                    parse_frame_object(frame, data, size, pos);
                    ++(*frame_index);
                    ++sprite->frame_count;

                    if(consume_char(data, size, pos, ',')) {
                        continue;
                    }
                    expect_char(data, size, pos, ']');
                    break;
                }
            }
        } else {
            skip_json_value(data, size, pos);
        }

        if(consume_char(data, size, pos, ',')) {
            continue;
        }
        expect_char(data, size, pos, '}');
        return;
    }
}

internal void parse_sprites_object(
    Atlas* atlas,
    AtlasFrame* frames,
    u8* data,
    u64 size,
    u64* pos
) {
    if(!expect_char(data, size, pos, '{')) {
        return;
    }

    u32 sprite_index = 0;
    u32 frame_index = 0;

    if(consume_char(data, size, pos, '}')) {
        return;
    }

    for(;;) {
        ASSERT(
            sprite_index < atlas->sprite_count,
            "Atlas sprite count mismatch!"
        );
        AtlasSprite* sprite = &atlas->sprites[sprite_index++];
        sprite->name = parse_json_string(data, size, pos);
        expect_char(data, size, pos, ':');
        parse_sprite_object(sprite, frames, &frame_index, data, size, pos);

        if(consume_char(data, size, pos, ',')) {
            continue;
        }
        expect_char(data, size, pos, '}');
        break;
    }

    ASSERT(sprite_index == atlas->sprite_count, "Atlas sprite count mismatch!");
}

internal void sort_sprites_by_name(Atlas* atlas) {
    for(u32 i = 1; i < atlas->sprite_count; ++i) {
        AtlasSprite key = atlas->sprites[i];
        i32 j = (i32)i - 1;
        while(j >= 0 && string_compare(atlas->sprites[j].name, key.name) > 0) {
            atlas->sprites[j + 1] = atlas->sprites[j];
            --j;
        }
        atlas->sprites[j + 1] = key;
    }
}

AtlasImage atlas_load_image(Arena* arena, char const* png_path) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(png_path != nullptr, "PNG path must not be null!");

    AtlasImage image = {};

#if OS_EMSCRIPTEN
    int em_width = 0;
    int em_height = 0;
    char* image_data =
        emscripten_get_preloaded_image_data(png_path, &em_width, &em_height);
    if(image_data == nullptr) {
        LOG_ERROR("Failed to get preloaded atlas image '%s'", png_path);
        return image;
    }

    image.width = (u32)em_width;
    image.height = (u32)em_height;
    u64 pixel_count = (u64)image.width * (u64)image.height * 4ULL;
    image.pixels = push_array_no_zero(arena, u8, pixel_count);
    memcpy(image.pixels, image_data, (usize)pixel_count);
    free(image_data);
#else
    SDL_Surface* loaded_surface = IMG_Load(png_path);
    if(loaded_surface == nullptr) {
        LOG_ERROR(
            "Failed to load atlas image '%s': %s",
            png_path,
            SDL_GetError()
        );
        return image;
    }

    SDL_Surface* rgba_surface =
        SDL_ConvertSurface(loaded_surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded_surface);
    if(rgba_surface == nullptr) {
        LOG_ERROR(
            "Failed to convert atlas image '%s' to RGBA32: %s",
            png_path,
            SDL_GetError()
        );
        return image;
    }

    image.width = (u32)rgba_surface->w;
    image.height = (u32)rgba_surface->h;
    u64 pixel_count = (u64)image.width * (u64)image.height * 4ULL;
    image.pixels = push_array_no_zero(arena, u8, pixel_count);
    for(u32 y = 0; y < image.height; ++y) {
        u8* src_row =
            (u8*)rgba_surface->pixels + (u64)y * (u64)rgba_surface->pitch;
        u8* dst_row = image.pixels + (u64)y * (u64)image.width * 4ULL;
        memcpy(dst_row, src_row, (usize)image.width * 4U);
    }

    SDL_DestroySurface(rgba_surface);
#endif

    return image;
}

Atlas atlas_load(Arena* arena, char const* json_path, char const* png_path) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(json_path != nullptr, "JSON path must not be null!");
    ASSERT(png_path != nullptr, "PNG path must not be null!");

    Atlas atlas = {};
    atlas.image_path = png_path;

    FileData json_file = os_read_file(arena, json_path);
    if(json_file.data == nullptr || json_file.size == 0) {
        return atlas;
    }

    atlas.sprite_count =
        count_token_occurrences(json_file.data, json_file.size, "\"frames\"");
    u32 total_frame_count =
        count_token_occurrences(json_file.data, json_file.size, "\"frame\"");
    atlas.sprites = push_array(arena, AtlasSprite, atlas.sprite_count);
    AtlasFrame* frames = push_array(arena, AtlasFrame, total_frame_count);

    u64 pos = 0;
    if(expect_char(json_file.data, json_file.size, &pos, '{')) {
        if(!consume_char(json_file.data, json_file.size, &pos, '}')) {
            for(;;) {
                String key =
                    parse_json_string(json_file.data, json_file.size, &pos);
                expect_char(json_file.data, json_file.size, &pos, ':');

                if(string_equals_cstr(key, "atlas")) {
                    parse_atlas_metadata(
                        &atlas,
                        json_file.data,
                        json_file.size,
                        &pos
                    );
                } else if(string_equals_cstr(key, "sprites")) {
                    parse_sprites_object(
                        &atlas,
                        frames,
                        json_file.data,
                        json_file.size,
                        &pos
                    );
                } else {
                    skip_json_value(json_file.data, json_file.size, &pos);
                }

                if(consume_char(json_file.data, json_file.size, &pos, ',')) {
                    continue;
                }
                expect_char(json_file.data, json_file.size, &pos, '}');
                break;
            }
        }
    }

    sort_sprites_by_name(&atlas);
    LOG_INFO("Atlas loaded: %u sprites", atlas.sprite_count);
    return atlas;
}

AtlasSprite* atlas_find(Atlas* atlas, String name) {
    if(atlas == nullptr || atlas->sprites == nullptr) {
        return nullptr;
    }

    i32 left = 0;
    i32 right = (i32)atlas->sprite_count - 1;
    while(left <= right) {
        i32 mid = left + (right - left) / 2;
        AtlasSprite* sprite = &atlas->sprites[mid];
        i32 cmp = string_compare(sprite->name, name);
        if(cmp == 0) {
            return sprite;
        }
        if(cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return nullptr;
}

AtlasFrame* atlas_frame(AtlasSprite* sprite, u32 frame_index) {
    if(sprite == nullptr || sprite->frames == nullptr) {
        return nullptr;
    }
    if(frame_index >= sprite->frame_count) {
        LOG_ERROR("Atlas frame index out of bounds: %u", frame_index);
        return nullptr;
    }
    return &sprite->frames[frame_index];
}
