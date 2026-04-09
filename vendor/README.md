# Vendor Layout

Store vendored third-party dependencies by host platform and architecture.

## Native Libraries

Use one directory per host target directly under `vendor/`:

```text
vendor/
  linux-x64/
    SDL3/
    SDL3_image/
    webgpu/
    emsdk/
  win32/
    SDL3/
    SDL3_image/
    webgpu/
  win32-x64/
    emsdk/
  macos-arm64/
    SDL3/
    SDL3_image/
    webgpu/
    emsdk/
```

Keep each package as a self-contained install/unpack output.

- `SDL3/include/SDL3/SDL.h`
- `SDL3/lib/` or `SDL3/lib64/` on Linux and macOS
- `SDL3/lib/<arch>/` on Windows, for example `SDL3/lib/x64/`
- `SDL3_image/include/SDL3_image/SDL_image.h`
- `webgpu/include/webgpu/webgpu.h`
- `webgpu/lib/` with the platform-specific binaries
- `webgpu/lib/<arch>/` on Windows, for example `webgpu/lib/arm64/`
- `emsdk/` as the host-side Emscripten SDK for that platform

## Notes

- Native builds look for Linux and macOS dependencies in `vendor/<platform>-<arch>/...`.
- Native Windows builds look for shared dependencies in `vendor/win32/...` and choose the matching `lib/<arch>/` directory.
- Web builds on Windows still look for emsdk in `vendor/win32-x64/emsdk/`.
