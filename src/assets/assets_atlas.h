#pragma once

#include "base/base_mod.h"

struct AtlasFrame {
    vec2 uv_min;
    vec2 uv_max;
    f32 width_px;
    f32 height_px;
};

struct Atlas {
    u32 atlas_width;
    u32 atlas_height;
    char const* image_path;
};

struct AtlasImage {
    u32 width;
    u32 height;
    u8* pixels;
};

Atlas atlas_load(char const* png_path);
AtlasImage atlas_load_image(Arena* arena, char const* png_path);
u32 atlas_animation_frame_count(u32 animation_id);
AtlasFrame atlas_animation_frame(u32 animation_id, u32 frame_index);
