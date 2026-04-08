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

struct WorldCamera {
    vec2 position;
    f32 zoom;
};

struct WorldSpritePass {
    WorldCamera camera;
    SpriteInstance* sprites;
    u32 sprite_count;
    u32 sprite_capacity;
};

struct BackgroundPass {};
struct TextPass {};
struct UiPass {};
struct DebugPass {};

struct RenderFrame {
    vec4 clear_color;
    BackgroundPass backgrounds;
    WorldSpritePass world_sprites;
    TextPass text;
    UiPass ui;
    DebugPass debug;
};

WorldSpritePass create_world_sprite_pass(Arena* arena, u32 initial_capacity);
void world_sprite_pass_reset(WorldSpritePass* pass);
void push_world_sprite(
    WorldSpritePass* pass,
    vec2 uv_min,
    vec2 uv_max,
    vec2 position,
    vec2 size,
    f32 depth,
    vec4 tint
);

RenderFrame create_render_frame(
    Arena* arena,
    u32 initial_world_sprite_capacity
);
void render_frame_reset(RenderFrame* frame);
