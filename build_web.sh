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
web_dir="$root_dir/bin/web"
src_dir="$root_dir/src"
vendor_dir="$root_dir/vendor"

# --- Setup Emscripten ---------------------------------------------------------
emsdk_dir="$vendor_dir/emsdk"
emsdk_env="$emsdk_dir/emsdk_env.sh"

if [ -f "$emsdk_env" ]; then
    source "$emsdk_env"
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Run: ./build.sh vendor" >&2
    exit 1
fi

# --- Emscripten uses built-in SDL3 + WebGPU, no vendor needed ----------------

# --- Compile/Link Line Definitions -------------------------------------------
common="-std=c++11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -I$src_dir"
common="$common -sUSE_SDL=3 --use-port=emdawnwebgpu -sALLOW_MEMORY_GROWTH=1"
common="$common -sASYNCIFY=1"
common="$common -DNDEBUG -DSDL_PLATFORM_EMSCRIPTEN"

if [ "${debug:-}" = "1" ]; then
    compile="emcc -g -O0 $common -sASSERTIONS=2"
else
    compile="emcc -O2 $common"
fi

# Output HTML with embedded assets
shell_file="${EMSDK:-$emsdk_dir}/upstream/emscripten/html/shell_minimal.html"
link="--shell-file $shell_file"
link="$link --preload-file $root_dir/assets@/assets"
link="$link -sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
link="$link -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"

# --- Prep Directories --------------------------------------------------------
mkdir -p "$web_dir"

# --- Build Targets -----------------------------------------------------------
didbuild=""
if [ -z "${game:-}" ] && [ -z "${run:-}" ]; then game=1; fi

if [ "${game:-}" = "1" ]; then
    didbuild=1
    $compile "$src_dir/app/game_main.cpp" $link -o "$web_dir/game.html"
    echo "built $web_dir/game.html"
fi

# --- Run ----------------------------------------------------------------------
if [ "${run:-}" = "1" ]; then
    if [ ! -f "$web_dir/game.html" ]; then
        echo "game.html not found. Build first with: ./build_web.sh" >&2
        exit 1
    fi
    echo "serving at http://localhost:6931/game.html"
    emrun "$web_dir/game.html"
fi

# --- Warn On No Builds -------------------------------------------------------
if [ -z "$didbuild" ] && [ -z "${run:-}" ]; then
    echo "[WARNING] no valid build target. usage: ./build_web.sh [game] [debug|release] [run]" >&2
    exit 1
fi
