#include <SDL3/SDL.h>

#if OS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include "base/base_mod.cpp"
#include "os/os_mod.cpp"
#include "draw/draw_mod.cpp"
#include "game/game_mod.cpp"
#include "render/render_mod.cpp"

struct App {
    SDL_Window* window;
    WebGPURenderer renderer;
    GameState* game;
    Arena* arena;
    bool running;
    u32 width;
    u32 height;
};

internal void app_tick(void* arg) {
    App* app = (App*)arg;
    if(!app || !app->running) {
        return;
    }

    // Poll events
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_EVENT_QUIT:
                app->running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                u32 nw = (u32)event.window.data1;
                u32 nh = (u32)event.window.data2;
                if(nw > 0 && nh > 0) {
                    app->width = nw;
                    app->height = nh;
                }
                break;
            }
        }
    }

    // Update game
    f64 dt = 1.0 / 60.0;  // Fixed timestep for now
    game_update(app->game, dt);

    // Render
    if(begin_frame(&app->renderer, app->width, app->height)) {
        render_submit(&app->renderer, &app->game->cmd_buffer);
        end_frame(&app->renderer);
    }

#if OS_EMSCRIPTEN
    if(!app->running) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Initialize SDL
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_FATAL("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    // Create arena
    Arena* arena = arena_alloc(
#if OS_EMSCRIPTEN
        4 * MB,  // Smaller arena for Emscripten
        64 * KB
#else
        64 * MB,
        64 * KB
#endif
    );

    App app = {};
    app.arena = arena;
    app.running = true;
    app.width = 960;
    app.height = 540;

    // Create window
    Uint64 window_flags = SDL_WINDOW_RESIZABLE;
#if !OS_EMSCRIPTEN
    window_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

    app.window = SDL_CreateWindow(
        "WebGPU Game",
        (int)app.width,
        (int)app.height,
        window_flags
    );

    if(!app.window) {
        LOG_FATAL("Failed to create window: %s", SDL_GetError());
        return 1;
    }

#if !OS_EMSCRIPTEN
    int pw, ph;
    SDL_GetWindowSizeInPixels(app.window, &pw, &ph);
    app.width = (u32)pw;
    app.height = (u32)ph;
#endif

    // Initialize WebGPU
    app.renderer = init_webgpu(app.window);
    if(!app.renderer.internal_state) {
        LOG_FATAL("Failed to initialize WebGPU renderer");
        return 1;
    }

    // Initialize game
    app.game = init_game_state(arena);

    LOG_INFO("WebGPU Game started (%dx%d)", app.width, app.height);

    // Main loop
#if OS_EMSCRIPTEN
    emscripten_set_main_loop_arg(app_tick, &app, 0, true);
#else
    while(app.running) {
        app_tick(&app);
    }
#endif

    // Cleanup
    cleanup_webgpu(&app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    arena_release(arena);

    LOG_INFO("WebGPU Game exited");
    return 0;
}
