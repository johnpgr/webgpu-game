#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
for arg in "$@"; do
  if echo "$arg" | grep -qE '^[a-z_]+$'; then
    eval "$arg=1"
  else
    echo "unknown argument: $arg" >&2; exit 1
  fi
done
if [ "${release:-}" != "1" ]; then debug=1; fi
if [ "${debug:-}" = "1" ];   then echo "[debug mode]"; fi
if [ "${release:-}" = "1" ]; then echo "[release mode]"; fi

# --- Paths -------------------------------------------------------------------
root_dir="$(pwd)"
bin_dir="$root_dir/bin"
src_dir="$root_dir/src"
vendor_dir="$root_dir/vendor"

# --- Download Vendor Dependencies ---------------------------------------------
if [ "${vendor:-}" = "1" ]; then
  echo "=== Downloading vendor dependencies ==="

  # Detect platform for downloads
  arch="$(uname -m)"
  case "$arch" in
    x86_64)  dl_arch="x86_64" ;;
    aarch64) dl_arch="aarch64" ;;
    arm64)   dl_arch="aarch64" ;;
    *)       echo "unsupported arch: $arch" >&2; exit 1 ;;
  esac

  case "$(uname -s)" in
    Darwin) dl_platform="macos" ;;
    Linux)  dl_platform="linux" ;;
    *)      echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
  esac

  # --- SDL3 ---
  sdl3_inc="$vendor_dir/SDL3/include/SDL3/SDL.h"
  sdl3_lib="$vendor_dir/SDL3/lib/libSDL3.a"
  if [ ! -f "$sdl3_lib" ]; then
    sdl3_lib="$vendor_dir/SDL3/lib64/libSDL3.a"
  fi
  if [ -f "$sdl3_inc" ] && [ -f "$sdl3_lib" ]; then
    echo "[SDL3] Already installed, skipping..."
  else
    SDL3_VERSION="3.4.4"
    SDL3_BASE="https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}"

    echo "[SDL3] Downloading SDL ${SDL3_VERSION} source..."
    mkdir -p "$vendor_dir/sdl3_tmp"
    curl -fSL -o "$vendor_dir/sdl3_tmp/sdl3.tar.gz" \
      "${SDL3_BASE}/SDL3-${SDL3_VERSION}.tar.gz"

    echo "[SDL3] Extracting headers..."
    tar -xzf "$vendor_dir/sdl3_tmp/sdl3.tar.gz" -C "$vendor_dir/sdl3_tmp"
    sdl3_src_dir="$vendor_dir/sdl3_tmp/SDL3-${SDL3_VERSION}"

    mkdir -p "$vendor_dir/SDL3/include"
    cp -r "$sdl3_src_dir/include/SDL3" "$vendor_dir/SDL3/include/"

    echo "[SDL3] Building SDL3 from source (this may take a moment)..."
    build_sdl3_dir="$vendor_dir/sdl3_tmp/build"
    mkdir -p "$build_sdl3_dir"
    if command -v cmake >/dev/null 2>&1; then
      # Wayland-only build (disable X11 dependencies)
      cmake -S "$sdl3_src_dir" -B "$build_sdl3_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="-fPIC" \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DCMAKE_INSTALL_PREFIX="$vendor_dir/SDL3" \
        -DCMAKE_PREFIX_PATH="$vendor_dir/SDL3" \
        -DSDL_X11=OFF \
        -DSDL_X11_XCURSOR=OFF \
        -DSDL_X11_XDBE=OFF \
        -DSDL_X11_XINPUT2=OFF \
        -DSDL_X11_XRANDR=OFF \
        -DSDL_X11_XSCRNSAVER=OFF \
        -DSDL_X11_XSHAPE=OFF \
        -DSDL_X11_XVM=OFF \
        -DSDL_WAYLAND=ON
      cmake --build "$build_sdl3_dir" --config Release -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
      cmake --install "$build_sdl3_dir" --config Release
    else
      echo "[SDL3] cmake not found, building via configure..."
      (cd "$sdl3_src_dir" && ./configure --prefix="$vendor_dir/SDL3" --disable-x11 --enable-wayland --enable-static --disable-shared CFLAGS="-fPIC" && make -j"$(nproc 2>/dev/null || echo 4)" && make install)
    fi

    rm -rf "$vendor_dir/sdl3_tmp"
    echo "[SDL3] Done."
  fi

  # --- wgpu-native (WebGPU C API) ---
  wgpu_header="$vendor_dir/webgpu/include/webgpu/webgpu.h"
  if [ -f "$wgpu_header" ]; then
    echo "[wgpu-native] Already installed, skipping..."
  else
    WGPU_NATIVE_VERSION="27.0.4.0"
    WGPU_NATIVE_BASE="https://github.com/gfx-rs/wgpu-native/releases/download/v${WGPU_NATIVE_VERSION}"

    if [ "$dl_platform" = "macos" ]; then
      wgpu_artifact="wgpu-macos-${dl_arch}-release.zip"
    else
      wgpu_artifact="wgpu-linux-${dl_arch}-release.zip"
    fi

    echo "[wgpu-native] Downloading wgpu-native v${WGPU_NATIVE_VERSION} (${wgpu_artifact})..."
    mkdir -p "$vendor_dir/wgpu_tmp"
    curl -fSL -o "$vendor_dir/wgpu_tmp/wgpu.zip" \
      "${WGPU_NATIVE_BASE}/${wgpu_artifact}"

    echo "[wgpu-native] Extracting..."
    unzip -qo "$vendor_dir/wgpu_tmp/wgpu.zip" -d "$vendor_dir/wgpu_tmp/extracted"

    # Find the extracted directory structure
    wgpu_extracted="$vendor_dir/wgpu_tmp/extracted"
    # Check if there's a single wrapping directory (e.g., wgpu-linux-x86_64-release/)
    wgpu_inner=$(find "$wgpu_extracted" -mindepth 1 -maxdepth 1 -type d | head -1)
    # If the zip has include/ and lib/ at root (no wrapping dir), use extracted dir directly
    if [ -z "$wgpu_inner" ] || [ -d "$wgpu_extracted/include" ]; then
      wgpu_inner="$wgpu_extracted"
    fi

    mkdir -p "$vendor_dir/webgpu/include/webgpu"
    mkdir -p "$vendor_dir/webgpu/lib"

    # Copy headers
    if [ -d "$wgpu_inner/include/webgpu" ]; then
      cp -r "$wgpu_inner/include/webgpu" "$vendor_dir/webgpu/include/"
    elif [ -d "$wgpu_inner/include" ]; then
      cp -r "$wgpu_inner/include/"* "$vendor_dir/webgpu/include/"
    fi

    # Copy libraries
    if [ -d "$wgpu_inner/lib" ]; then
      cp "$wgpu_inner/lib/"* "$vendor_dir/webgpu/lib/"
    fi

    rm -rf "$vendor_dir/wgpu_tmp"
    echo "[wgpu-native] Done."
  fi

  # --- Emscripten SDK ---
  emsdk_dir="$vendor_dir/emsdk"
  emcc_bin="$emsdk_dir/upstream/emscripten/emcc"
  if [ -f "$emcc_bin" ]; then
    echo "[emscripten] Already installed, skipping..."
  else
    echo "[emscripten] Installing Emscripten SDK..."
    if [ ! -d "$emsdk_dir" ]; then
      git clone https://github.com/emscripten-core/emsdk.git "$emsdk_dir"
    else
      (cd "$emsdk_dir" && git pull)
    fi
    (cd "$emsdk_dir" && ./emsdk install latest && ./emsdk activate latest)
    echo "[emscripten] Done."
  fi

  echo "=== Vendor dependencies installed to $vendor_dir/==="
  echo ""
  echo "Directory structure:"
  find "$vendor_dir" -type f | head -20
  exit 0
fi

# --- Detect Platform ---------------------------------------------------------
case "$(uname -s)" in
  Darwin) platform=macos ;;
  Linux)  platform=linux ;;
  *)      echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

# --- Find WebGPU (wgpu-native) + SDL3 ----------------------------------------
webgpu_inc="$root_dir/vendor/webgpu/include"
webgpu_lib="$root_dir/vendor/webgpu/lib"

if [ ! -f "$webgpu_inc/webgpu/webgpu.h" ]; then
    echo "WebGPU headers not found at $webgpu_inc/webgpu/webgpu.h" >&2
    echo "Run: ./build.sh vendor   to download dependencies" >&2
    exit 1
fi

# Try to find SDL3: vendor dir first, then pkg-config, then system paths
sdl3_cflags=""
sdl3_libs=""
sdl3_vendor_inc="$root_dir/vendor/SDL3/include"
sdl3_vendor_lib="$root_dir/vendor/SDL3/lib"
sdl3_vendor_lib64="$root_dir/vendor/SDL3/lib64"
sdl3_vendor_static=""

# Look for static library in lib64 or lib
if [ -f "$sdl3_vendor_lib64/libSDL3.a" ]; then
    sdl3_vendor_static="$sdl3_vendor_lib64/libSDL3.a"
elif [ -f "$sdl3_vendor_lib/libSDL3.a" ]; then
    sdl3_vendor_static="$sdl3_vendor_lib/libSDL3.a"
fi

if [ -f "$sdl3_vendor_inc/SDL3/SDL.h" ]; then
    sdl3_cflags="-I$sdl3_vendor_inc"
    # Prefer static library if available
    if [ -n "$sdl3_vendor_static" ]; then
        sdl3_libs="$sdl3_vendor_static"
    else
        sdl3_libs="-L$sdl3_vendor_lib -lSDL3 -Wl,-rpath,$sdl3_vendor_lib"
    fi
elif pkg-config --exists sdl3 2>/dev/null; then
    sdl3_cflags=$(pkg-config --cflags sdl3)
    sdl3_libs=$(pkg-config --libs sdl3)
else
    for d in /usr/local/include /opt/homebrew/include /usr/include; do
        if [ -f "$d/SDL3/SDL.h" ]; then
            sdl3_cflags="-I$d"
            break
        fi
    done
    for d in /usr/local/lib /opt/homebrew/lib /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu; do
        if [ -f "$d/libSDL3.so" ] || [ -f "$d/libSDL3.dylib" ]; then
            sdl3_libs="-L$d -lSDL3"
            break
        fi
    done
fi

if [ -z "$sdl3_cflags" ] || [ -z "$sdl3_libs" ]; then
    echo "SDL3 not found. Install SDL3 or run: ./build.sh vendor" >&2
    exit 1
fi

# --- Compile/Link Line Definitions -------------------------------------------
compiler="${CXX:-clang++}"
common="-std=c++11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -I$src_dir -I$webgpu_inc"

if [ "$platform" = "macos" ]; then
    common="$common -DSDL_PLATFORM_MACOS"
    link_os="-framework Cocoa -framework IOKit -framework CoreVideo"
else
    common="$common -DSDL_PLATFORM_LINUX"
    link_os="-ldl -lm -lX11 -lXrandr"
fi

if [ "${debug:-}" = "1" ];   then compile="$compiler -g -O0 $common $sdl3_cflags"; fi
if [ "${release:-}" = "1" ]; then compile="$compiler -O2 -DNDEBUG $common $sdl3_cflags"; fi

link="$webgpu_lib/libwgpu_native.a $sdl3_libs $link_os -lpthread"

# --- Prep Directories --------------------------------------------------------
mkdir -p "$bin_dir"

# --- Copy Assets -------------------------------------------------------------
if [ -d "$root_dir/assets" ]; then
    cp -r "$root_dir/assets" "$bin_dir/"
fi

# --- Build Targets -----------------------------------------------------------
didbuild=""
if [ -z "${game:-}" ] && [ -z "${compdb:-}" ]; then game=1; fi

if [ "${game:-}" = "1" ]; then
    didbuild=1
    $compile "$src_dir/app/game_main.cpp" $link -o "$bin_dir/game"
    echo "built $bin_dir/game"
fi

# --- Compdb ------------------------------------------------------------------
if [ "${compdb:-}" = "1" ]; then
    didbuild=1
    build_log="$(mktemp)"
    trap 'rm -f "$build_log"' EXIT
    args="game"
    [ "${debug:-}" = "1" ] && args="$args debug"
    [ "${release:-}" = "1" ] && args="$args release"
    PS4='' bash -x "$0" $args >"$build_log" 2>&1
    compiledb -f -o "$root_dir/compile_commands.json" -p "$build_log"
    echo "generated $root_dir/compile_commands.json"
fi

# --- Warn On No Builds -------------------------------------------------------
if [ -z "$didbuild" ]; then
    echo "[WARNING] no valid build target. usage: ./build.sh [vendor] [game] [debug|release] [compdb]" >&2
    exit 1
fi