#pragma once

#include "assets/assets_mod.h"
#include "base/base_mod.h"
#include "draw/draw_mod.h"

#define GAME_API_VERSION 1

struct GameState {
    Arena* arena;
    RenderFrame render_frame;
    Atlas* atlas;
    f64 time;
    bool running;
};

extern "C" {
typedef GameState* (*game_init_fn)(Arena* arena, Atlas* atlas);
typedef void (*game_update_fn)(GameState* game, f64 dt);
typedef u32 (*game_get_api_version_fn)(void);
}

#if OS_WINDOWS
#define GAME_DLL_EXPORT extern "C" __declspec(dllexport)
#else
#define GAME_DLL_EXPORT extern "C"
#endif

GAME_DLL_EXPORT GameState* game_init(Arena* arena, Atlas* atlas);
GAME_DLL_EXPORT void game_update(GameState* game, f64 dt);
GAME_DLL_EXPORT u32 game_get_api_version(void);