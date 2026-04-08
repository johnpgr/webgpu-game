#include "draw/draw_core.h"

FrameState create_frame_state(Arena* arena, u32 initial_capacity) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(initial_capacity > 0, "Initial sprite capacity must be non-zero!");

    FrameState result = {};
    result.clear_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    result.camera_pos = vec2(0.0f, 0.0f);
    result.camera_zoom = 1.0f;
    result.sprites = push_array(arena, SpriteInstance, initial_capacity);
    result.sprite_capacity = initial_capacity;
    return result;
}

void frame_state_reset(FrameState* frame) {
    ASSERT(frame != nullptr, "Frame state must not be null!");
    frame->sprite_count = 0;
}

void push_sprite(
    FrameState* frame,
    vec2 uv_min,
    vec2 uv_max,
    vec2 position,
    vec2 size,
    f32 depth,
    vec4 tint
) {
    ASSERT(frame != nullptr, "Frame state must not be null!");
    ASSERT(
        frame->sprite_count < frame->sprite_capacity,
        "Sprite buffer overflow!"
    );

    SpriteInstance* instance = &frame->sprites[frame->sprite_count++];
    instance->uv_min = uv_min;
    instance->uv_max = uv_max;
    instance->position = position;
    instance->size = size;
    instance->depth = depth;
    instance->tint = tint;
}
