#include <SDL3/SDL.h>

#if OS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include "base/base_mod.cpp"
#include "os/os_mod.cpp"
#include "draw/draw_mod.cpp"
#include "assets/assets_mod.cpp"
#include "render/render_mod.cpp"
#if OS_EMSCRIPTEN
#include "game/game_mod.cpp"
#else
#include "game/game_dll.h"
#endif

#if !OS_EMSCRIPTEN

struct LoadedGameCode {
    SDL_SharedObject* library;
    game_init_fn game_init;
    game_update_fn game_update;
    game_get_api_version_fn game_get_api_version;
    u64 last_write_time;
    char source_path[512];
    char temp_path[512];
    bool valid;
};

internal LoadedGameCode load_game_code(char const* source_dll_path) {
    LoadedGameCode result = {};

    u64 source_time = os_get_file_modified_time(source_dll_path);
    if(source_time == 0) {
        LOG_ERROR("Game DLL not found: %s", source_dll_path);
        return result;
    }

    size_t path_len = SDL_strlen(source_dll_path);
    if(path_len >= sizeof(result.source_path) - 1) {
        LOG_ERROR("Game DLL path too long");
        return result;
    }

    SDL_memcpy(result.source_path, source_dll_path, path_len + 1);

    static u32 load_counter = 0;
    load_counter++;
    char temp_path[512];
    SDL_snprintf(
        temp_path,
        sizeof(temp_path),
        "%s.tmp%u",
        source_dll_path,
        load_counter
    );

    size_t temp_len = SDL_strlen(temp_path);
    if(temp_len >= sizeof(result.temp_path) - 1) {
        LOG_ERROR("Temp DLL path too long");
        return result;
    }
    SDL_memcpy(result.temp_path, temp_path, temp_len + 1);

    if(!os_copy_file(source_dll_path, result.temp_path)) {
        LOG_ERROR("Failed to copy game DLL to temp path");
        return result;
    }

    result.library = SDL_LoadObject(result.temp_path);
    if(!result.library) {
        LOG_ERROR("Failed to load game DLL: %s", SDL_GetError());
        os_delete_file(result.temp_path);
        return result;
    }

    result.game_init =
        (game_init_fn)SDL_LoadFunction(result.library, "game_init");
    result.game_update =
        (game_update_fn)SDL_LoadFunction(result.library, "game_update");
    result.game_get_api_version = (game_get_api_version_fn)
        SDL_LoadFunction(result.library, "game_get_api_version");

    if(!result.game_init || !result.game_update ||
       !result.game_get_api_version) {
        LOG_ERROR(
            "Failed to load game functions: init=%p update=%p version=%p",
            result.game_init,
            result.game_update,
            result.game_get_api_version
        );
        SDL_UnloadObject(result.library);
        os_delete_file(result.temp_path);
        return result;
    }

    u32 api_version = result.game_get_api_version();
    if(api_version != GAME_API_VERSION) {
        LOG_ERROR(
            "Game DLL API version mismatch: got %u, expected %u",
            api_version,
            GAME_API_VERSION
        );
        SDL_UnloadObject(result.library);
        os_delete_file(result.temp_path);
        return result;
    }

    result.last_write_time = source_time;
    result.valid = true;
    LOG_INFO(
        "Loaded game DLL: %s (temp: %s)",
        source_dll_path,
        result.temp_path
    );
    return result;
}

internal void unload_game_code(LoadedGameCode* code) {
    if(code->library) {
        SDL_UnloadObject(code->library);
        code->library = nullptr;
    }
    if(code->temp_path[0] != '\0') {
        os_delete_file(code->temp_path);
    }
    code->game_init = nullptr;
    code->game_update = nullptr;
    code->game_get_api_version = nullptr;
    code->valid = false;
}

internal bool check_game_code_changed(LoadedGameCode* code) {
    u64 current_time = os_get_file_modified_time(code->source_path);
    if(current_time == 0 || current_time == code->last_write_time) {
        return false;
    }
    return true;
}

#endif

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
#if !OS_EMSCRIPTEN
    LoadedGameCode game_code;
#endif
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

#if !OS_EMSCRIPTEN
    if(check_game_code_changed(&app->game_code)) {
        LOG_INFO("Game DLL changed, reloading...");
        LoadedGameCode new_code = load_game_code(app->game_code.source_path);
        if(new_code.valid) {
            unload_game_code(&app->game_code);
            app->game_code = new_code;
            u32 api_version = app->game_code.game_get_api_version();
            if(api_version != GAME_API_VERSION) {
                LOG_ERROR("API version mismatch after reload");
            } else {
                LOG_INFO("Game DLL reloaded successfully");
            }
        } else {
            LOG_ERROR("Failed to reload game DLL, keeping previous version");
        }
    }
#endif

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

#if OS_EMSCRIPTEN
    game_update(app->game, dt);
#else
    if(app->game_code.valid && app->game_code.game_update) {
        app->game_code.game_update(app->game, dt);
    }
#endif

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

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_FATAL("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    Arena* arena = arena_alloc(
#if OS_EMSCRIPTEN
        4 * MB,
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

    app.renderer = init_webgpu(app.window, arena, atlas);
    if(!app.renderer.internal_state) {
        LOG_FATAL("Failed to initialize WebGPU renderer");
        return 1;
    }

#if OS_EMSCRIPTEN
    app.game = game_init(arena, atlas);
    if(!app.game) {
        LOG_FATAL("Failed to initialize game state");
        return 1;
    }
#else
#if OS_WINDOWS
    char const* game_dll_path = "bin/game_code.dll";
#elif OS_MAC
    char const* game_dll_path = "bin/game_code.dylib";
#else
    char const* game_dll_path = "bin/game_code.so";
#endif
    app.game_code = load_game_code(game_dll_path);
    if(!app.game_code.valid) {
        LOG_FATAL("Failed to load game DLL");
        return 1;
    }

    app.game = app.game_code.game_init(arena, atlas);
    if(!app.game) {
        LOG_FATAL("Failed to initialize game state");
        return 1;
    }
#endif

    LOG_INFO("WebGPU Game started (%dx%d)", app.width, app.height);
    SDL_ShowWindow(app.window);

#if OS_EMSCRIPTEN
    emscripten_set_main_loop_arg(app_tick, &app, 0, true);
#else
    while(app.running) {
        app_tick(&app);
    }
#endif

#if !OS_EMSCRIPTEN
    unload_game_code(&app.game_code);
#endif

    cleanup_webgpu(&app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    arena_release(arena);

    LOG_INFO("WebGPU Game exited");
    return 0;
}