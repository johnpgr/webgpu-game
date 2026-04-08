#pragma once

#include "base/base_arena.h"

struct SpriteInstance {
    vec2 uv_min;
    vec2 uv_max;
    vec2 position;
    vec2 size;
    f32 depth;
    vec4 tint;
};
static_assert(sizeof(SpriteInstance) == 52, "SpriteInstance must be 52 bytes");

struct FrameState {
    vec4 clear_color;
    vec2 camera_pos;
    f32 camera_zoom;
    SpriteInstance* sprites;
    u32 sprite_count;
    u32 sprite_capacity;
};

FrameState create_frame_state(Arena* arena, u32 initial_capacity);
void frame_state_reset(FrameState* frame);
void push_sprite(
    FrameState* frame,
    vec2 uv_min,
    vec2 uv_max,
    vec2 position,
    vec2 size,
    f32 depth,
    vec4 tint
);
