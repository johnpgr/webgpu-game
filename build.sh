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
if [ -z "${game:-}" ] && [ -z "${web:-}" ] && [ -z "${dll:-}" ]; then
  game=1
fi

# --- Paths -------------------------------------------------------------------
root_dir="$(pwd)"
bin_dir="$root_dir/bin"
web_dir="$bin_dir/web"
src_dir="$root_dir/src"
vendor_dir="$root_dir/vendor"

case "$(uname -s)" in
  Darwin) host_platform="macos" ;;
  Linux)  host_platform="linux" ;;
  *)      echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

case "$(uname -m)" in
  x86_64)  host_arch="x64" ;;
  aarch64) host_arch="arm64" ;;
  arm64)   host_arch="arm64" ;;
  *)       echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

vendor_inc_dir="$vendor_dir/include"
vendor_lib_dir="$vendor_dir/lib/$host_platform-$host_arch"
vendor_emsdk_dir="$vendor_dir/emsdk"

warning_flags="-Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator"
didbuild=""

# --- Build Native Game (desktop) ---------------------------------------------
if [ "${game:-}" = "1" ] || [ "${dll:-}" = "1" ]; then
  platform="$host_platform"

  echo "[$platform-$host_arch]"

  # --- Find WebGPU (wgpu-native) ---------------------------------------------
  if [ ! -f "$vendor_inc_dir/webgpu/webgpu.h" ]; then
    echo "WebGPU headers not found at $vendor_inc_dir/webgpu/webgpu.h" >&2
    echo "Place vendored headers under $vendor_inc_dir/" >&2
    exit 1
  fi

  webgpu_cflags="-I$vendor_inc_dir"
  webgpu_libs="-L$vendor_lib_dir -lwgpu_native -Wl,-rpath,$vendor_lib_dir"

  # Try to find SDL3: vendor lib dir first, then pkg-config, then system paths
  sdl3_cflags=""
  sdl3_libs=""

  if [ -f "$vendor_inc_dir/SDL3/SDL.h" ] && { [ -f "$vendor_lib_dir/libSDL3.so.0" ] || [ -f "$vendor_lib_dir/libSDL3.dylib" ]; }; then
    sdl3_cflags="-I$vendor_inc_dir"
    sdl3_libs="-L$vendor_lib_dir -lSDL3 -Wl,-rpath,$vendor_lib_dir"
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
    echo "SDL3 not found. Expected vendored files under $vendor_lib_dir/ or an installed system package." >&2
    exit 1
  fi

  # --- Find SDL3_image -------------------------------------------------------
  sdl3_image_cflags=""
  sdl3_image_libs=""

  if [ -f "$vendor_inc_dir/SDL3_image/SDL_image.h" ] && { [ -f "$vendor_lib_dir/libSDL3_image.so.0" ] || [ -f "$vendor_lib_dir/libSDL3_image.dylib" ]; }; then
    sdl3_image_cflags="-I$vendor_inc_dir"
    sdl3_image_libs="-L$vendor_lib_dir -lSDL3_image -Wl,-rpath,$vendor_lib_dir"
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
    echo "SDL3_image not found. Expected vendored files under $vendor_lib_dir/ or an installed system package." >&2
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
    link_os=""
  fi

  if [ "${debug:-}" = "1" ];   then compile="$compiler -g -O0 $common $sdl3_cflags $sdl3_image_cflags"; fi
  if [ "${release:-}" = "1" ]; then compile="$compiler -O2 -DNDEBUG $common $sdl3_cflags $sdl3_image_cflags"; fi

  host_link="$webgpu_libs $sdl3_image_libs $sdl3_libs $link_os -lpthread"

  # For shared library compilation
  dll_compile="$compile -fPIC -shared"

  # --- Prep Directories ------------------------------------------------------
  mkdir -p "$bin_dir"

  if [ "${game:-}" = "1" ]; then
    # --- Copy Assets -----------------------------------------------------------
    if [ -d "$root_dir/assets" ]; then
      cp -r "$root_dir/assets" "$bin_dir/"
    fi

    # --- Copy shared libs to bin -----------------------------------------------
    for f in "$vendor_lib_dir"/libSDL3*.so* "$vendor_lib_dir"/libSDL3*.dylib* "$vendor_lib_dir"/libwgpu_native.so* "$vendor_lib_dir"/libwgpu_native.dylib*; do
      [ -f "$f" ] && cp "$f" "$bin_dir/" 2>/dev/null || true
    done
  fi

  # --- Build Native Targets --------------------------------------------------
  didbuild=1

  if [ "${game:-}" = "1" ]; then
    echo "Building game host executable..."
    $compile "$src_dir/app/game_main.cpp" $host_link -o "$bin_dir/game_host"
    echo "Built $bin_dir/game_host"
  fi

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
  emsdk_dir="$vendor_emsdk_dir"
  emsdk_env="$emsdk_dir/emsdk_env.sh"

  echo "[web]"

  if [ -f "$emsdk_env" ]; then
    # shellcheck disable=SC1090
    source "$emsdk_env"
  fi

  if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Expected emsdk at $vendor_dir/emsdk/ or on PATH." >&2
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
  echo "[WARNING] no valid build target. usage: ./build.sh [game|dll] [web|web_run] [debug|release]" >&2
    exit 1
fi
