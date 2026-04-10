#pragma once

#include "base/base_types.h"

struct SDL_Window;
struct Arena;
struct Atlas;
struct RenderFrame;

struct WebGPURenderer {
    void* internal_state; // opaque pointer to WebGPUState
};

WebGPURenderer init_webgpu(SDL_Window* window, Arena* arena, Atlas* atlas);
void cleanup_webgpu(WebGPURenderer* renderer);

// Returns false if surface is lost/invalid (should resize/recreate)
bool begin_frame(WebGPURenderer* renderer, u32 width, u32 height);
void render_submit(WebGPURenderer* renderer, RenderFrame* frame);
void end_frame(WebGPURenderer* renderer);
