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

#include "../../assets/sprites/atlas.h"
#include "assets/assets_atlas.h"

internal AtlasFrame atlas_frame_from_packed_sprite(AtlasSprite* sprite) {
    AtlasFrame frame = {};
    if(sprite == nullptr) {
        return frame;
    }

    frame.uv_min = vec2(sprite->u0, sprite->v0);
    frame.uv_max = vec2(sprite->u1, sprite->v1);
    frame.width_px = (f32)sprite->w;
    frame.height_px = (f32)sprite->h;
    return frame;
}

AtlasImage atlas_load_image(Arena* arena, const char* png_path) {
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

Atlas atlas_load(const char* png_path) {
    ASSERT(png_path != nullptr, "PNG path must not be null!");

    Atlas atlas = {};
    atlas.atlas_width = ATLAS_ATLAS_WIDTH;
    atlas.atlas_height = ATLAS_ATLAS_HEIGHT;
    atlas.image_path = png_path;

    LOG_INFO("Atlas loaded: %u animations", (u32)ANIMATION_ID_COUNT);
    return atlas;
}

u32 atlas_animation_frame_count(u32 animation_id) {
    if(animation_id >= (u32)ANIMATION_ID_COUNT) {
        LOG_ERROR("Atlas animation id out of bounds: %u", animation_id);
        return 0;
    }

    return (u32)atlas_animations[animation_id].frame_count;
}

AtlasFrame atlas_animation_frame(u32 animation_id, u32 frame_index) {
    AtlasFrame frame = {};
    if(animation_id >= (u32)ANIMATION_ID_COUNT) {
        LOG_ERROR("Atlas animation id out of bounds: %u", animation_id);
        return frame;
    }

    AtlasAnimation* animation =
        (AtlasAnimation*)&atlas_animations[animation_id];
    if(frame_index >= (u32)animation->frame_count) {
        LOG_ERROR(
            "Atlas frame index out of bounds: animation=%u frame=%u",
            animation_id,
            frame_index
        );
        return frame;
    }

    u32 sprite_id = (u32)animation->first + frame_index;
    ASSERT(sprite_id < (u32)SPRITE_ID_COUNT, "Atlas sprite id out of bounds!");
    return atlas_frame_from_packed_sprite(
        (AtlasSprite*)&atlas_sprites[sprite_id]
    );
}
