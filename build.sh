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

if [ "${web_run:-}" = "1" ]; then
  web=1
  run=1
fi

# Running implies web mode when no explicit target was provided.
if [ "${run:-}" = "1" ] && [ -z "${game:-}" ] && [ -z "${web:-}" ]; then
  web=1
fi

# Default target remains native game build.
if [ -z "${game:-}" ] && [ -z "${web:-}" ]; then
  game=1
fi

# --- Paths -------------------------------------------------------------------
root_dir="$(pwd)"
bin_dir="$root_dir/bin"
web_dir="$bin_dir/web"
src_dir="$root_dir/src"
vendor_dir="$root_dir/vendor"

# --- Download Vendor Dependencies ---------------------------------------------
if [ "${vendor:-}" = "1" ]; then
  echo "=== Downloading vendor dependencies ==="

  cmake_platform_args=()

  # Detect platform for downloads
  arch="$(uname -m)"
  case "$arch" in
    x86_64)  dl_arch="x86_64" ;;
    aarch64) dl_arch="aarch64" ;;
    arm64)   dl_arch="aarch64" ;;
    *)       echo "unsupported arch: $arch" >&2; exit 1 ;;
  esac

  case "$(uname -s)" in
    Darwin)
      dl_platform="macos"
      macos_sdk="$(xcrun --show-sdk-path)"
      if [ -z "$macos_sdk" ] || [ ! -d "$macos_sdk" ]; then
        echo "failed to locate macOS SDK with xcrun --show-sdk-path" >&2
        exit 1
      fi
      cmake_platform_args+=("-DCMAKE_OSX_SYSROOT=$macos_sdk")
      ;;
    Linux)  dl_platform="linux" ;;
    *)      echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
  esac

  # --- SDL3 ---
  sdl3_inc="$vendor_dir/SDL3/include/SDL3/SDL.h"
  if [ -f "$vendor_dir/SDL3/lib/libSDL3.so" ] || [ -f "$vendor_dir/SDL3/lib64/libSDL3.so" ] || [ -f "$vendor_dir/SDL3/lib/libSDL3.dylib" ]; then
    sdl3_lib_found=1
  fi
  if [ -f "$sdl3_inc" ] && [ "${sdl3_lib_found:-}" = "1" ]; then
    echo "[SDL3] Already installed, skipping..."
  else
    SDL3_VERSION="3.4.4"
    SDL3_BASE="https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}"

    echo "[SDL3] Downloading SDL ${SDL3_VERSION} source..."
    mkdir -p "$vendor_dir/sdl3_tmp"
    curl -fSL -o "$vendor_dir/sdl3_tmp/sdl3.tar.gz" \
      "${SDL3_BASE}/SDL3-${SDL3_VERSION}.tar.gz"

    echo "[SDL3] Extracting..."
    tar -xzf "$vendor_dir/sdl3_tmp/sdl3.tar.gz" -C "$vendor_dir/sdl3_tmp"
    sdl3_src_dir="$vendor_dir/sdl3_tmp/SDL3-${SDL3_VERSION}"

    echo "[SDL3] Building SDL3 from source (this may take a moment)..."
    build_sdl3_dir="$vendor_dir/sdl3_tmp/build"
    mkdir -p "$build_sdl3_dir"
    cmake -S "$sdl3_src_dir" -B "$build_sdl3_dir" \
      -DCMAKE_BUILD_TYPE=Release \
      "${cmake_platform_args[@]}" \
      -DSDL_SHARED=ON \
      -DSDL_STATIC=OFF \
      -DCMAKE_INSTALL_PREFIX="$vendor_dir/SDL3" \
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

    rm -rf "$vendor_dir/sdl3_tmp"
    echo "[SDL3] Done."
  fi

  # --- SDL3_image ---
  sdl3_image_inc="$vendor_dir/SDL3_image/include/SDL3_image/SDL_image.h"
  if [ -f "$vendor_dir/SDL3_image/lib/libSDL3_image.so" ] || [ -f "$vendor_dir/SDL3_image/lib64/libSDL3_image.so" ] || [ -f "$vendor_dir/SDL3_image/lib/libSDL3_image.dylib" ]; then
    sdl3_image_lib_found=1
  fi
  if [ -f "$sdl3_image_inc" ] && [ "${sdl3_image_lib_found:-}" = "1" ]; then
    echo "[SDL3_image] Already installed, skipping..."
  else
    SDL3_IMAGE_VERSION="3.4.2"
    SDL3_IMAGE_BASE="https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_IMAGE_VERSION}"

    echo "[SDL3_image] Downloading SDL_image ${SDL3_IMAGE_VERSION} source..."
    mkdir -p "$vendor_dir/sdl3_image_tmp"
    curl -fSL -o "$vendor_dir/sdl3_image_tmp/sdl3_image.tar.gz" \
      "${SDL3_IMAGE_BASE}/SDL3_image-${SDL3_IMAGE_VERSION}.tar.gz"

    echo "[SDL3_image] Extracting..."
    tar -xzf "$vendor_dir/sdl3_image_tmp/sdl3_image.tar.gz" -C "$vendor_dir/sdl3_image_tmp"
    sdl3_image_src_dir="$vendor_dir/sdl3_image_tmp/SDL3_image-${SDL3_IMAGE_VERSION}"

    echo "[SDL3_image] Fetching vendored dependencies..."
    (cd "$sdl3_image_src_dir/external" && sh download.sh --depth 1)

    echo "[SDL3_image] Building SDL3_image from source (this may take a moment)..."
    build_sdl3_image_dir="$vendor_dir/sdl3_image_tmp/build"
    mkdir -p "$build_sdl3_image_dir"
    cmake -S "$sdl3_image_src_dir" -B "$build_sdl3_image_dir" \
      -DCMAKE_BUILD_TYPE=Release \
      "${cmake_platform_args[@]}" \
      -DSDL_SHARED=ON \
      -DSDL_STATIC=OFF \
      -DCMAKE_INSTALL_PREFIX="$vendor_dir/SDL3_image" \
      -DCMAKE_PREFIX_PATH="$vendor_dir/SDL3" \
      -DSDLIMAGE_VENDORED=ON \
      -DSDLIMAGE_AVIF=OFF \
      -DSDLIMAGE_JXL=OFF \
      -DSDLIMAGE_TIF=OFF \
      -DSDLIMAGE_WEBP=OFF
    cmake --build "$build_sdl3_image_dir" --config Release -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    cmake --install "$build_sdl3_image_dir" --config Release

    rm -rf "$vendor_dir/sdl3_image_tmp"
    echo "[SDL3_image] Done."
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

warning_flags="-Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator"
didbuild=""

# --- Build Native Game (desktop) ---------------------------------------------
if [ "${game:-}" = "1" ]; then
  # --- Detect Platform -------------------------------------------------------
  case "$(uname -s)" in
  Darwin) platform=macos ;;
  Linux)  platform=linux ;;
  *)      echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
  esac

  echo "[$platform]"

  # --- Find WebGPU (wgpu-native) ---------------------------------------------
  webgpu_inc="$root_dir/vendor/webgpu/include"
  webgpu_lib="$root_dir/vendor/webgpu/lib"

  if [ ! -f "$webgpu_inc/webgpu/webgpu.h" ]; then
    echo "WebGPU headers not found at $webgpu_inc/webgpu/webgpu.h" >&2
    echo "Run: ./build.sh vendor   to download dependencies" >&2
    exit 1
  fi

  webgpu_cflags="-I$webgpu_inc"
  webgpu_libs="-L$webgpu_lib -lwgpu_native -Wl,-rpath,$webgpu_lib"

  # Try to find SDL3: vendor dir first, then pkg-config, then system paths
  sdl3_cflags=""
  sdl3_libs=""
  sdl3_vendor_inc="$root_dir/vendor/SDL3/include"
  sdl3_vendor_lib="$root_dir/vendor/SDL3/lib"
  sdl3_vendor_lib64="$root_dir/vendor/SDL3/lib64"

  if [ -f "$sdl3_vendor_inc/SDL3/SDL.h" ]; then
    sdl3_cflags="-I$sdl3_vendor_inc"
    if [ -f "$sdl3_vendor_lib64/libSDL3.so" ] || [ -f "$sdl3_vendor_lib64/libSDL3.dylib" ]; then
      sdl3_libs="-L$sdl3_vendor_lib64 -lSDL3 -Wl,-rpath,$sdl3_vendor_lib64"
    elif [ -f "$sdl3_vendor_lib/libSDL3.so" ] || [ -f "$sdl3_vendor_lib/libSDL3.dylib" ]; then
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

  # --- Find SDL3_image -------------------------------------------------------
  sdl3_image_cflags=""
  sdl3_image_libs=""
  sdl3_image_vendor_inc="$root_dir/vendor/SDL3_image/include"
  sdl3_image_vendor_lib="$root_dir/vendor/SDL3_image/lib"
  sdl3_image_vendor_lib64="$root_dir/vendor/SDL3_image/lib64"

  if [ -f "$sdl3_image_vendor_inc/SDL3_image/SDL_image.h" ]; then
    sdl3_image_cflags="-I$sdl3_image_vendor_inc"
    if [ -f "$sdl3_image_vendor_lib64/libSDL3_image.so" ] || [ -f "$sdl3_image_vendor_lib64/libSDL3_image.dylib" ]; then
      sdl3_image_libs="-L$sdl3_image_vendor_lib64 -lSDL3_image -Wl,-rpath,$sdl3_image_vendor_lib64"
    elif [ -f "$sdl3_image_vendor_lib/libSDL3_image.so" ] || [ -f "$sdl3_image_vendor_lib/libSDL3_image.dylib" ]; then
      sdl3_image_libs="-L$sdl3_image_vendor_lib -lSDL3_image -Wl,-rpath,$sdl3_image_vendor_lib"
    fi
  elif pkg-config --exists sdl3-image 2>/dev/null; then
    sdl3_image_cflags=$(pkg-config --cflags sdl3-image)
    sdl3_image_libs=$(pkg-config --libs sdl3-image)
  else
    for d in /usr/local/include /opt/homebrew/include /usr/include; do
      if [ -f "$d/SDL3_image/SDL_image.h" ]; then
        sdl3_image_cflags="-I$d"
        break
      fi
    done
    for d in /usr/local/lib /opt/homebrew/lib /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu; do
      if [ -f "$d/libSDL3_image.so" ] || [ -f "$d/libSDL3_image.dylib" ]; then
        sdl3_image_libs="-L$d -lSDL3_image"
        break
      fi
    done
  fi

  if [ -z "$sdl3_image_cflags" ] || [ -z "$sdl3_image_libs" ]; then
    echo "SDL3_image not found. Run: ./build.sh vendor" >&2
    exit 1
  fi

  # --- Compile/Link Line Definitions -----------------------------------------
  compiler="${CXX:-clang++}"
  common="-std=c++20 $warning_flags -I$src_dir $webgpu_cflags"

  if [ "$platform" = "macos" ]; then
    common="$common -DSDL_PLATFORM_MACOS"
    link_os="-framework Cocoa -framework IOKit -framework CoreVideo"
  else
    common="$common -DSDL_PLATFORM_LINUX"
    link_os="-ldl -lm -lX11 -lXrandr"
  fi

  if [ "${debug:-}" = "1" ];   then compile="$compiler -g -O0 $common $sdl3_cflags $sdl3_image_cflags"; fi
  if [ "${release:-}" = "1" ]; then compile="$compiler -O2 -DNDEBUG $common $sdl3_cflags $sdl3_image_cflags"; fi

  host_link="$webgpu_libs $sdl3_image_libs $sdl3_libs $link_os -lpthread"

  # For shared library compilation
  dll_compile="$compile -fPIC -shared"

  # --- Prep Directories ------------------------------------------------------
  mkdir -p "$bin_dir"

  # --- Copy Assets -----------------------------------------------------------
  if [ -d "$root_dir/assets" ]; then
    cp -r "$root_dir/assets" "$bin_dir/"
  fi

  # --- Copy shared libs to bin -----------------------------------------------
  for d in "$root_dir/vendor/SDL3/lib64" "$root_dir/vendor/SDL3/lib" "$root_dir/vendor/SDL3_image/lib64" "$root_dir/vendor/SDL3_image/lib" "$root_dir/vendor/webgpu/lib"; do
    if [ -d "$d" ]; then
      for f in "$d"/libSDL3*.so* "$d"/libSDL3*.dylib* "$d"/libwgpu_native.so* "$d"/libwgpu_native.dylib*; do
        [ -f "$f" ] && cp "$f" "$bin_dir/" 2>/dev/null || true
      done
    fi
  done

  # --- Build Native Targets --------------------------------------------------
  didbuild=1

  echo "Building game host executable..."
  $compile "$src_dir/app/game_main.cpp" $host_link -o "$bin_dir/game_host"
  echo "Built $bin_dir/game_host"

  echo "Building game shared library..."
  if [ "$platform" = "macos" ]; then
    dll_ext=".dylib"
  else
    dll_ext=".so"
  fi
  $dll_compile "$src_dir/game/game_dll_main.cpp" $webgpu_cflags $sdl3_cflags $sdl3_image_cflags -o "$bin_dir/game_code$dll_ext"
  echo "Built $bin_dir/game_code$dll_ext"
fi

# --- Build Web Game (Emscripten) ---------------------------------------------
if [ "${web:-}" = "1" ]; then
  emsdk_dir="$vendor_dir/emsdk"
  emsdk_env="$emsdk_dir/emsdk_env.sh"

  echo "[web]"

  if [ -f "$emsdk_env" ]; then
    # shellcheck disable=SC1090
    source "$emsdk_env"
  fi

  if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Run: ./build.sh vendor" >&2
    exit 1
  fi

  web_common="-std=c++11 $warning_flags -I$src_dir"
  web_common="$web_common -sUSE_SDL=3 --use-port=emdawnwebgpu -sALLOW_MEMORY_GROWTH=1"
  web_common="$web_common -sINITIAL_MEMORY=167772160"
  web_common="$web_common -sASYNCIFY=1"
  web_common="$web_common -DNDEBUG -DSDL_PLATFORM_EMSCRIPTEN"

  if [ "${debug:-}" = "1" ]; then
    web_compile="emcc -g -O0 $web_common -sASSERTIONS=2"
  else
    web_compile="emcc -O2 $web_common"
  fi

  shell_file="$src_dir/app/game_main.html"
  web_link="--shell-file $shell_file"
  web_link="$web_link --preload-file $root_dir/assets@/assets"
  web_link="$web_link --use-preload-plugins"
  web_link="$web_link -sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
  web_link="$web_link -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"

  mkdir -p "$web_dir"

  didbuild=1
  $web_compile "$src_dir/app/game_main.cpp" $web_link -o "$web_dir/game.html"
  echo "Built $web_dir/game.html"
fi

# --- Run Web Build -----------------------------------------------------------
if [ "${run:-}" = "1" ]; then
  if [ ! -f "$web_dir/game.html" ]; then
    echo "game.html not found. Build first with: ./build.sh web" >&2
    exit 1
  fi
  echo "serving at http://localhost:6931/game.html"
  emrun "$web_dir/game.html"
fi

# --- Warn On No Builds -------------------------------------------------------
if [ -z "$didbuild" ] && [ "${run:-}" != "1" ]; then
  echo "[WARNING] no valid build target. usage: ./build.sh [vendor] [game] [web|web_run] [debug|release]" >&2
    exit 1
fi
