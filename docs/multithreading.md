# Multithreading

This project does not use threads right now.

When multithreading is needed later, use SDL3 threading APIs directly instead
of reintroducing a custom thread abstraction.

## Native Targets

For native builds, prefer the SDL3 primitives:

- `SDL_CreateThread`
- `SDL_WaitThread`
- `SDL_CreateMutex`
- `SDL_LockMutex`
- `SDL_UnlockMutex`
- `SDL_CreateCondition`
- `SDL_WaitCondition`
- `SDL_SignalCondition`
- `SDL_BroadcastCondition`
- `SDL_GetNumLogicalCPUCores`

SDL already maps these to the correct platform implementation on Windows,
Linux, and macOS.

## Web Target

For Emscripten, SDL3 threads are backed by pthreads.

Important consequence:

- If the web build is compiled without pthread support, `SDL_CreateThread()`
  exists but thread creation fails at runtime.
- There is no automatic fallback where SDL threads silently become browser
  workers in a non-threaded build.

## What To Enable Later

If this project needs SDL threads on web, add a dedicated threaded web build.

Minimum compiler change:

```bash
emcc ... -pthread
```

Common Emscripten thread-related variants:

- `-pthread` enables the pthreads model used by SDL threads.
- `-sWASM_WORKERS` enables Emscripten's separate Wasm Workers API.

This project should treat those as different choices:

- Use `-pthread` for portable SDL/native-style threading.
- Use Wasm Workers only for web-specific code that intentionally depends on
  Emscripten APIs instead of SDL.

## Runtime Requirements On Web

Threaded web builds need more than compiler flags.

- The app must be served with cross-origin isolation enabled.
- The server must send COOP/COEP headers or the app will fail to start with
  shared-memory threads.
- Rendering must stay on the main thread.
- A threaded web build and a non-threaded web build should be treated as
  separate build modes.

## Suggested Future Build Mode

If threading is introduced, extend `build_web.sh` with an explicit opt-in mode
such as `pthreads`.

That mode should:

- add `-pthread`
- keep the main loop on the browser main thread
- document the required COOP/COEP headers for local dev and deployment
- avoid changing the default non-threaded web build

## Current Recommendation

Until the game actually needs background work, keep the current web build
single-threaded.

When threading becomes necessary:

1. decide whether the need is portable SDL threading or web-only worker logic
2. if portable, add a separate pthread-enabled web target
3. keep rendering and browser-facing work on the main thread
