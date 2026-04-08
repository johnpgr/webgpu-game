#include "draw/draw_core.h"

WorldSpritePass create_world_sprite_pass(Arena* arena, u32 initial_capacity) {
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(initial_capacity > 0, "Initial sprite capacity must be non-zero!");

    WorldSpritePass result = {};
    result.camera.position = vec2(0.0f, 0.0f);
    result.camera.zoom = 1.0f;
    result.sprites = push_array(arena, SpriteInstance, initial_capacity);
    result.sprite_capacity = initial_capacity;
    return result;
}

void world_sprite_pass_reset(WorldSpritePass* pass) {
    ASSERT(pass != nullptr, "World sprite pass must not be null!");
    pass->sprite_count = 0;
}

void push_world_sprite(
    WorldSpritePass* pass,
    vec2 uv_min,
    vec2 uv_max,
    vec2 position,
    vec2 size,
    f32 depth,
    vec4 tint
) {
    ASSERT(pass != nullptr, "World sprite pass must not be null!");
    ASSERT(
        pass->sprite_count < pass->sprite_capacity,
        "Sprite buffer overflow!"
    );

    SpriteInstance* instance = &pass->sprites[pass->sprite_count++];
    instance->uv_min = uv_min;
    instance->uv_max = uv_max;
    instance->position = position;
    instance->size = size;
    instance->depth = depth;
    instance->tint = tint;
}

RenderFrame create_render_frame(
    Arena* arena,
    u32 initial_world_sprite_capacity
) {
    ASSERT(arena != nullptr, "Arena must not be null!");

    RenderFrame result = {};
    result.clear_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    result.world_sprites =
        create_world_sprite_pass(arena, initial_world_sprite_capacity);
    return result;
}

void render_frame_reset(RenderFrame* frame) {
    ASSERT(frame != nullptr, "Render frame must not be null!");
    world_sprite_pass_reset(&frame->world_sprites);
}
