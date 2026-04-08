#pragma once

#include "base/base_mod.h"

struct AtlasFrame {
    vec2 uv_min;
    vec2 uv_max;
    f32 width_px;
    f32 height_px;
};

struct AtlasSprite {
    String name;
    AtlasFrame* frames;
    u32 frame_count;
};

struct Atlas {
    AtlasSprite* sprites;
    u32 sprite_count;
    u32 atlas_width;
    u32 atlas_height;
    u8* pixel_data;
};

Atlas atlas_load(Arena* arena, char const* json_path, char const* png_path);
AtlasSprite* atlas_find(Atlas* atlas, String name);
AtlasFrame* atlas_frame(AtlasSprite* sprite, u32 frame_index);
