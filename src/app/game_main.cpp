#include <SDL3/SDL.h>

#if OS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include "base/base_mod.cpp"
#include "os/os_mod.cpp"
#include "draw/draw_mod.cpp"
#include "assets/assets_mod.cpp"
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
    u64 perf_frequency;
    u64 last_frame_counter;
    u64 target_frame_ns;
};

internal u64 app_get_target_frame_ns(SDL_Window* window) {
#if OS_EMSCRIPTEN
    (void)window;
    return 0;
#else
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if(display == 0) {
        return SDL_NS_PER_SECOND / 60ULL;
    }

    SDL_DisplayMode const* mode = SDL_GetCurrentDisplayMode(display);
    if(mode == nullptr) {
        return SDL_NS_PER_SECOND / 60ULL;
    }

    f64 refresh_hz = 0.0;
    if(mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0) {
        refresh_hz = (f64)mode->refresh_rate_numerator /
                     (f64)mode->refresh_rate_denominator;
    } else if(mode->refresh_rate > 0.0f) {
        refresh_hz = (f64)mode->refresh_rate;
    }

    if(refresh_hz <= 1.0) {
        refresh_hz = 60.0;
    }

    return (u64)((f64)SDL_NS_PER_SECOND / refresh_hz);
#endif
}

internal f64 app_begin_frame(App* app, u64* frame_start_counter) {
    ASSERT(app != nullptr, "App must not be null!");
    ASSERT(
        frame_start_counter != nullptr,
        "Frame start output must not be null!"
    );

    *frame_start_counter = SDL_GetPerformanceCounter();
    if(app->last_frame_counter == 0) {
        app->last_frame_counter = *frame_start_counter;
        return 1.0 / 60.0;
    }

    u64 elapsed = *frame_start_counter - app->last_frame_counter;
    app->last_frame_counter = *frame_start_counter;

    f64 dt = (f64)elapsed / (f64)app->perf_frequency;
    return clamp(dt, 1.0 / 1000.0, 0.1);
}

internal void app_end_frame(App* app, u64 frame_start_counter) {
#if OS_EMSCRIPTEN
    (void)app;
    (void)frame_start_counter;
#else
    if(app->target_frame_ns == 0) {
        return;
    }

    u64 frame_end_counter = SDL_GetPerformanceCounter();
    u64 elapsed_counter = frame_end_counter - frame_start_counter;
    u64 elapsed_ns =
        (elapsed_counter * SDL_NS_PER_SECOND) / app->perf_frequency;

    if(elapsed_ns < app->target_frame_ns) {
        SDL_DelayPrecise(app->target_frame_ns - elapsed_ns);
    }
#endif
}

internal void app_tick(void* arg) {
    App* app = (App*)arg;
    if(!app || !app->running) {
        return;
    }

    u64 frame_start_counter = 0;
    f64 dt = app_begin_frame(app, &frame_start_counter);

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
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                u32 nw = (u32)event.window.data1;
                u32 nh = (u32)event.window.data2;
                if(nw > 0 && nh > 0) {
                    app->width = nw;
                    app->height = nh;
                }
                break;
            }
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                app->target_frame_ns = app_get_target_frame_ns(app->window);
                break;
        }
    }

    // Update game
    game_update(app->game, dt);

    // Render
    if(begin_frame(&app->renderer, app->width, app->height)) {
        render_submit(&app->renderer, &app->game->render_frame);
        end_frame(&app->renderer);
    }

    app_end_frame(app, frame_start_counter);

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
        4 * MB, // Smaller arena for Emscripten
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
    app.perf_frequency = SDL_GetPerformanceFrequency();

    // Create window
    u64 window_flags = SDL_WINDOW_HIDDEN;
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
    app.target_frame_ns = app_get_target_frame_ns(app.window);
#endif

    Atlas* atlas = push_struct(arena, Atlas);
    *atlas = atlas_load(
        arena,
        "assets/sprites/atlas.json",
        "assets/sprites/atlas.png"
    );

    // Initialize WebGPU
    app.renderer = init_webgpu(app.window, arena, atlas);
    if(!app.renderer.internal_state) {
        LOG_FATAL("Failed to initialize WebGPU renderer");
        return 1;
    }

    // Initialize game
    app.game = init_game_state(arena, atlas);

    LOG_INFO("WebGPU Game started (%dx%d)", app.width, app.height);
    SDL_ShowWindow(app.window);

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
