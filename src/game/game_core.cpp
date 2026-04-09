#include "../../assets/sprites/atlas.h"
#include "game/game_core.h"

internal f32 demo_lane_min_y = -80.0f;
internal f32 demo_lane_max_y = 20.0f;

internal vec2 animation_size(u32 animation_id) {
    AtlasFrame frame = atlas_animation_frame(animation_id, 0);
    return vec2(frame.width_px, frame.height_px);
}

internal void configure_directional_animations(
    Entity* entity,
    u32 idle_side,
    u32 idle_up,
    u32 idle_down,
    u32 move_side,
    u32 move_up,
    u32 move_down
) {
    entity->properties = entity_set_property(
        entity->properties,
        ENTITY_PROPERTY_DIRECTIONAL_ANIMATION
    );
    entity->idle_animation_side = idle_side;
    entity->idle_animation_up = idle_up;
    entity->idle_animation_down = idle_down;
    entity->move_animation_side = move_side;
    entity->move_animation_up = move_up;
    entity->move_animation_down = move_down;
}

internal void build_demo_background(RenderFrame* frame, f32 camera_x) {
    BackgroundPass* backgrounds = &frame->backgrounds;
    backgrounds->camera.position = vec2(camera_x, 0.0f);
    backgrounds->camera.zoom = 2.0f;

    f32 left = camera_x - 2304.0f;
    f32 width = 4608.0f;

    push_background_rect(
        backgrounds,
        vec2(left, 96.0f),
        vec2(width, 256.0f),
        0.99f,
        vec4(0.51f, 0.80f, 0.96f, 1.0f)
    );
    push_background_rect(
        backgrounds,
        vec2(left, 36.0f),
        vec2(width, 60.0f),
        0.97f,
        vec4(0.38f, 0.72f, 0.29f, 1.0f)
    );
    push_background_rect(
        backgrounds,
        vec2(left, 10.0f),
        vec2(width, 26.0f),
        0.95f,
        vec4(0.76f, 0.76f, 0.73f, 1.0f)
    );
    push_background_rect(
        backgrounds,
        vec2(left, -46.0f),
        vec2(width, 56.0f),
        0.93f,
        vec4(0.28f, 0.29f, 0.31f, 1.0f)
    );
    push_background_rect(
        backgrounds,
        vec2(left, -72.0f),
        vec2(width, 26.0f),
        0.89f,
        vec4(0.81f, 0.80f, 0.77f, 1.0f)
    );
    push_background_rect(
        backgrounds,
        vec2(left, -256.0f),
        vec2(width, 184.0f),
        0.87f,
        vec4(0.24f, 0.63f, 0.18f, 1.0f)
    );

    for(i32 i = -40; i <= 40; ++i) {
        push_background_rect(
            backgrounds,
            vec2(camera_x + (f32)(i * 96) - 24.0f, -22.0f),
            vec2(48.0f, 6.0f),
            0.91f,
            vec4(0.94f, 0.85f, 0.25f, 1.0f)
        );
    }
}

EXPORT GameState* game_init(Arena* arena, Atlas* atlas) {
    GameState* game = push_struct(arena, GameState);
    game->arena = arena;
    game->render_frame = create_render_frame(arena, 4096);
    game->atlas = atlas;
    game->time = 0.0;
    game->running = true;
    game->world = create_game_world(arena, 1024);

    u32 player_idle_side =
        ENTITIES_CHARACTERS_BODY_A_ANIMATIONS_IDLE_BASE_IDLE_SIDE;
    u32 player_idle_up =
        ENTITIES_CHARACTERS_BODY_A_ANIMATIONS_IDLE_BASE_IDLE_UP;
    u32 player_idle_down =
        ENTITIES_CHARACTERS_BODY_A_ANIMATIONS_IDLE_BASE_IDLE_DOWN;
    u32 player_run_side =
        ENTITIES_CHARACTERS_BODY_A_ANIMATIONS_RUN_BASE_RUN_SIDE;
    u32 player_run_up = ENTITIES_CHARACTERS_BODY_A_ANIMATIONS_RUN_BASE_RUN_UP;
    u32 player_run_down =
        ENTITIES_CHARACTERS_BODY_A_ANIMATIONS_RUN_BASE_RUN_DOWN;
    vec2 player_size = animation_size(player_run_side);

    game->player = spawn_player_entity(
        game->world,
        vec2(0.0f, 16.0f),
        player_idle_side,
        player_size
    );
    Entity* player = world_get_entity(game->world, game->player);
    if(player != nullptr) {
        configure_directional_animations(
            player,
            player_idle_side,
            player_idle_up,
            player_idle_down,
            player_run_side,
            player_run_up,
            player_run_down
        );
        player->render_layer = ENTITY_RENDER_LAYER_SORTED;
    }

    u32 enemy_idle = ENTITIES_MOBS_SKELETON_CREW_SKELETON_WARRIOR_IDLE_IDLE;
    u32 enemy_run = ENTITIES_MOBS_SKELETON_CREW_SKELETON_WARRIOR_RUN_RUN;
    vec2 enemy_size = animation_size(enemy_run);
    game->enemy = spawn_moving_entity(
        game->world,
        vec2(220.0f, 24.0f),
        vec2(80.0f, 0.0f),
        enemy_run,
        enemy_size,
        0.0f
    );
    Entity* enemy = world_get_entity(game->world, game->enemy);
    if(enemy != nullptr) {
        configure_directional_animations(
            enemy,
            enemy_idle,
            enemy_idle,
            enemy_idle,
            enemy_run,
            enemy_run,
            enemy_run
        );
        enemy->render_layer = ENTITY_RENDER_LAYER_SORTED;
    }

    u32 tree_animation = ENVIRONMENT_PROPS_STATIC_TREES_MODEL_02_SIZE_05;
    vec2 tree_size = animation_size(tree_animation);
    game->tree = world_spawn_entity(game->world);
    Entity* tree = world_get_entity(game->world, game->tree);
    if(tree != nullptr) {
        tree->properties =
            entity_set_property(tree->properties, ENTITY_PROPERTY_WORLD_SPRITE);
        tree->position = vec2(320.0f, 44.0f);
        tree->size = tree_size;
        tree->sprite_anchor = vec2(tree_size.x * 0.5f, 4.0f);
        tree->render_layer = ENTITY_RENDER_LAYER_SORTED;
        tree->animation_id = tree_animation;
        tree->tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return game;
}

EXPORT void game_update(GameState* game, GameInput const* input, f64 dt) {
    render_frame_reset(&game->render_frame);
    game->time += dt;

    Entity* player = world_get_entity(game->world, game->player);
    if(player != nullptr) {
        f32 move_x = input ? input->move_x : 0.0f;
        f32 move_y = input ? input->move_y : 0.0f;
        player->velocity = vec2(move_x * 140.0f, move_y * 70.0f);
    }

    Entity* enemy = world_get_entity(game->world, game->enemy);
    if(enemy != nullptr) {
        if(enemy->position.x <= 220.0f) {
            enemy->position.x = 220.0f;
            enemy->velocity.x = 80.0f;
        } else if(enemy->position.x >= 380.0f) {
            enemy->position.x = 380.0f;
            enemy->velocity.x = -80.0f;
        }
        enemy->velocity.y = 0.0f;
    }

    world_update(game->world, dt);

    if(player != nullptr) {
        player->position.y =
            clamp(player->position.y, demo_lane_min_y, demo_lane_max_y);
    }
    if(enemy != nullptr) {
        enemy->position.y = 24.0f;
        if(enemy->position.x <= 220.0f) {
            enemy->position.x = 220.0f;
            enemy->velocity.x = 80.0f;
        } else if(enemy->position.x >= 380.0f) {
            enemy->position.x = 380.0f;
            enemy->velocity.x = -80.0f;
        }
    }

    f32 camera_x = 0.0f;
    if(player != nullptr) {
        camera_x = player->position.x;
    }

    game->render_frame.clear_color = vec4(0.51f, 0.80f, 0.96f, 1.0f);
    game->render_frame.world_sprites.camera.position = vec2(camera_x, 0.0f);
    game->render_frame.world_sprites.camera.zoom = 2.0f;

    build_demo_background(&game->render_frame, camera_x);
    world_extract_render(game->world, &game->render_frame);
}

EXPORT u32 game_get_api_version(void) {
    return GAME_API_VERSION;
}
