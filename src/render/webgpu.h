#pragma once

#include "base/base_mod.h"
#include "draw/draw_mod.h"

struct SDL_Window;

struct WebGPURenderer {
    void* internal_state;  // opaque pointer to WebGPUState
};

WebGPURenderer init_webgpu(SDL_Window* window);
void cleanup_webgpu(WebGPURenderer* renderer);

// Returns false if surface is lost/invalid (should resize/recreate)
bool begin_frame(WebGPURenderer* renderer, u32 width, u32 height);
void render_submit(WebGPURenderer* renderer, PushCmdBuffer* cmd_buffer);
void end_frame(WebGPURenderer* renderer);
