#include "game/game_core.h"

GAME_DLL_EXPORT GameState* game_init(Arena* arena, Atlas* atlas) {
    GameState* game = push_struct(arena, GameState);
    game->arena = arena;
    game->render_frame = create_render_frame(arena, 4096);
    game->atlas = atlas;
    game->time = 0.0;
    game->running = true;
    return game;
}

GAME_DLL_EXPORT void game_update(GameState* game, f64 dt) {
    render_frame_reset(&game->render_frame);
    game->time += dt;

    game->render_frame.clear_color = vec4(0.08f, 0.08f, 0.12f, 1.0f);
    game->render_frame.world_sprites.camera.position = vec2(0.0f, 0.0f);
    game->render_frame.world_sprites.camera.zoom = 1.0f;

    AtlasSprite* bone_sprite =
        atlas_find(game->atlas, string_lit("Weapons/Bone/Bone"));
    AtlasSprite* bonfire_sprite = atlas_find(
        game->atlas,
        string_lit("Environment/Structures/Stations/Bonfire/Bonfire_01")
    );

    if(bone_sprite != nullptr) {
        AtlasFrame* bone_frame = atlas_frame(bone_sprite, 0);
        if(bone_frame != nullptr) {
            push_world_sprite(
                &game->render_frame.world_sprites,
                bone_frame->uv_min,
                bone_frame->uv_max,
                vec2(-140.0f, -92.0f),
                vec2(bone_frame->width_px, bone_frame->height_px),
                0.75f,
                vec4(0.55f, 0.65f, 0.95f, 1.0f)
            );

            push_world_sprite(
                &game->render_frame.world_sprites,
                bone_frame->uv_min,
                bone_frame->uv_max,
                vec2(-48.0f, -44.0f),
                vec2(bone_frame->width_px, bone_frame->height_px),
                0.20f,
                vec4(1.0f, 1.0f, 1.0f, 0.95f)
            );
        }
    }

    if(bonfire_sprite != nullptr && bonfire_sprite->frame_count > 0) {
        u32 frame_index = (u32)(game->time * 8.0) % bonfire_sprite->frame_count;
        AtlasFrame* bonfire_frame = atlas_frame(bonfire_sprite, frame_index);
        if(bonfire_frame != nullptr) {
            push_world_sprite(
                &game->render_frame.world_sprites,
                bonfire_frame->uv_min,
                bonfire_frame->uv_max,
                vec2(-12.0f, -16.0f),
                vec2(128.0f, 128.0f),
                0.20f,
                vec4(1.0f, 0.95f, 0.9f, 1.0f)
            );
        }
    }
}

GAME_DLL_EXPORT u32 game_get_api_version(void) {
    return GAME_API_VERSION;
}