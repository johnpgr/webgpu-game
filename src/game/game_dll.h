#pragma once

#include "draw/draw_core.h"
#include "game/game_entity.h"

struct Atlas;

#define GAME_API_VERSION 3

struct GameInput {
    f32 move_x;
    f32 move_y;
};

struct GameState {
    Arena* arena;
    RenderFrame render_frame;
    Atlas* atlas;
    GameWorld* world;
    EntityHandle player;
    EntityHandle enemy;
    EntityHandle tree;
    f64 time;
    bool running;
};

extern "C" {
typedef GameState* (*game_init_fn)(Arena* arena, Atlas* atlas);
typedef void (*game_update_fn)(GameState* game, GameInput* input, f64 dt);
typedef u32 (*game_get_api_version_fn)(void);
}

EXPORT GameState* game_init(Arena* arena, Atlas* atlas);
EXPORT void game_update(GameState* game, GameInput* input, f64 dt);
EXPORT u32 game_get_api_version(void);
