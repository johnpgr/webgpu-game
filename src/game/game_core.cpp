#include <math.h>

#include "game/game_core.h"

GameState* init_game_state(Arena* arena) {
    GameState* game = push_struct(arena, GameState);
    game->arena = arena;
    game->cmd_buffer = create_push_cmd_buffer(arena, 256 * 1024);
    game->time = 0.0;
    game->running = true;
    return game;
}

void game_update(GameState* game, f64 dt) {
    push_cmd_buffer_reset(&game->cmd_buffer);
    game->time += dt;

    push_clear(&game->cmd_buffer, vec4(0.1f, 0.1f, 0.3f, 1.0f));

    f32 t = (f32)game->time;
    f32 r = 0.5f + 0.5f * sinf(t * 2.0f);
    f32 g = 0.5f + 0.5f * sinf(t * 3.0f + 2.0f);
    f32 b = 0.5f + 0.5f * sinf(t * 4.0f + 4.0f);

    push_rect(&game->cmd_buffer, vec2(0.0f, 0.0f), vec2(0.5f, 0.5f), vec4(r, g, b, 1.0f));
}
