#pragma once

#if defined(SDL_PLATFORM_WIN32) && __has_include(<windows.h>)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
