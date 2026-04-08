#pragma once

#include "assets/assets_mod.h"
#include "base/base_mod.h"
#include "draw/draw_mod.h"

struct GameState {
    Arena* arena;
    FrameState frame;
    Atlas* atlas;
    f64 time;
    bool running;
};

GameState* init_game_state(Arena* arena, Atlas* atlas);
void game_update(GameState* game, f64 dt);
