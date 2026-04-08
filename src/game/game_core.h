#pragma once

#include "base/base_mod.h"
#include "draw/draw_mod.h"

struct GameState {
    Arena* arena;
    PushCmdBuffer cmd_buffer;
    f64 time;
    bool running;
};

GameState* init_game_state(Arena* arena);
void game_update(GameState* game, f64 dt);
