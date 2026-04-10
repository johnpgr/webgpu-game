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
  Linux) host_platform="linux" ;;
  MINGW*|MSYS*|CYGWIN*) host_platform="win32" ;;
  *) echo "unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

case "$(uname -m)" in
  x86_64|amd64) host_arch="x64" ;;
  aarch64|arm64) host_arch="arm64" ;;
  i686|i386) host_arch="x86" ;;
  *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

vendor_inc_dir="$vendor_dir/include"
vendor_lib_dir="$vendor_dir/lib/$host_platform-$host_arch"
vendor_emsdk_dir="$vendor_dir/emsdk"

warning_flags="-Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator"
didbuild=""

ensure_not_lfs_pointer() {
  file="$1"
  first_line=""

  if [ ! -f "$file" ]; then
    return 0
  fi

  IFS= read -r first_line < "$file" || first_line=""
  if [ "$first_line" = "version https://git-lfs.github.com/spec/v1" ]; then
    echo "Git LFS object missing for $(basename "$file")" >&2
    echo "Run: git lfs pull" >&2
    exit 1
  fi
}

ensure_native_pch() {
  pch_compile="$1"
  pch_header="$2"
  pch_file="$3"
  pch_signature_file="$4"
  pch_signature="$pch_compile|$pch_header"

  if [ ! -f "$pch_file" ] || [ "$pch_header" -nt "$pch_file" ] || [ ! -f "$pch_signature_file" ] || [ "$(<"$pch_signature_file")" != "$pch_signature" ]; then
    echo "Building native PCH: $(basename "$pch_file")"
    $pch_compile -Winvalid-pch -x c++-header "$pch_header" -o "$pch_file"
    printf '%s' "$pch_signature" > "$pch_signature_file"
  fi
}

copy_if_needed() {
  src="$1"
  dst_dir="$2"
  dst="$dst_dir/$(basename "$src")"

  if [ ! -f "$dst" ] || [ "$src" -nt "$dst" ]; then
    cp -p "$src" "$dst_dir/"
  fi
}

# --- Build Native Game (desktop) ---------------------------------------------
if [ "${game:-}" = "1" ] || [ "${dll:-}" = "1" ]; then
  platform="$host_platform"

  echo "[$platform-$host_arch]"

  if [ ! -f "$vendor_inc_dir/webgpu/webgpu.h" ]; then
    echo "WebGPU headers not found at $vendor_inc_dir/webgpu/webgpu.h" >&2
    echo "Place vendored headers under $vendor_inc_dir/" >&2
    exit 1
  fi

  webgpu_cflags="-I$vendor_inc_dir"
  sdl3_cflags=""
  sdl3_libs=""
  sdl3_image_cflags=""
  sdl3_image_libs=""
  webgpu_runtime=""
  vendor_rpath=""

  if [ "$platform" = "win32" ]; then
    if [ ! -f "$vendor_inc_dir/SDL3/SDL.h" ]; then
      echo "SDL3 headers not found at $vendor_inc_dir/SDL3/SDL.h" >&2
      echo "Place native Windows headers under $vendor_inc_dir/" >&2
      exit 1
    fi

    if [ ! -f "$vendor_inc_dir/SDL3_image/SDL_image.h" ]; then
      echo "SDL3_image headers not found at $vendor_inc_dir/SDL3_image/SDL_image.h" >&2
      echo "Place native Windows headers under $vendor_inc_dir/" >&2
      exit 1
    fi

    if [ ! -f "$vendor_lib_dir/wgpu_native.dll.lib" ] || [ ! -f "$vendor_lib_dir/wgpu_native.dll" ]; then
      echo "Shared WebGPU $host_arch libraries not found at $vendor_lib_dir/wgpu_native.dll(.lib)" >&2
      echo "Place native Windows shared libraries under $vendor_lib_dir/" >&2
      exit 1
    fi

    if [ ! -f "$vendor_lib_dir/SDL3.lib" ]; then
      echo "SDL3 $host_arch libraries not found at $vendor_lib_dir/SDL3.lib" >&2
      echo "Place native Windows libraries under $vendor_lib_dir/" >&2
      exit 1
    fi

    if [ ! -f "$vendor_lib_dir/SDL3_image.lib" ]; then
      echo "SDL3_image $host_arch libraries not found at $vendor_lib_dir/SDL3_image.lib" >&2
      echo "Place native Windows libraries under $vendor_lib_dir/" >&2
      exit 1
    fi

    ensure_not_lfs_pointer "$vendor_lib_dir/wgpu_native.dll.lib"
    ensure_not_lfs_pointer "$vendor_lib_dir/wgpu_native.dll"
    webgpu_libs="$vendor_lib_dir/wgpu_native.dll.lib"
    webgpu_runtime="$vendor_lib_dir/wgpu_native.dll"

    ensure_not_lfs_pointer "$vendor_lib_dir/SDL3.lib"
    ensure_not_lfs_pointer "$vendor_lib_dir/SDL3_image.lib"

    sdl3_cflags="-I$vendor_inc_dir"
    sdl3_libs="$vendor_lib_dir/SDL3.lib"
    sdl3_image_cflags="-I$vendor_inc_dir"
    sdl3_image_libs="$vendor_lib_dir/SDL3_image.lib"
  else
    if [ "${install:-}" = "1" ]; then
      vendor_rpath="-Wl,-rpath,\$ORIGIN"
    else
      vendor_rpath="-Wl,-rpath,$vendor_lib_dir"
    fi

    if [ -f "$vendor_lib_dir/libwgpu_native.so" ]; then
      ensure_not_lfs_pointer "$vendor_lib_dir/libwgpu_native.so"
      webgpu_libs="$vendor_lib_dir/libwgpu_native.so"
      webgpu_runtime="$vendor_lib_dir/libwgpu_native.so"
    elif [ -f "$vendor_lib_dir/libwgpu_native.dylib" ]; then
      ensure_not_lfs_pointer "$vendor_lib_dir/libwgpu_native.dylib"
      webgpu_libs="$vendor_lib_dir/libwgpu_native.dylib"
      webgpu_runtime="$vendor_lib_dir/libwgpu_native.dylib"
    else
      echo "Shared WebGPU native library not found under $vendor_lib_dir/" >&2
      exit 1
    fi

    if [ -f "$vendor_inc_dir/SDL3/SDL.h" ] && { [ -f "$vendor_lib_dir/libSDL3.so.0" ] || [ -f "$vendor_lib_dir/libSDL3.dylib" ]; }; then
      sdl3_cflags="-I$vendor_inc_dir"
      if [ -f "$vendor_lib_dir/libSDL3.so.0" ]; then
        sdl3_libs="$vendor_lib_dir/libSDL3.so.0"
      else
        sdl3_libs="$vendor_lib_dir/libSDL3.dylib"
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
      echo "SDL3 not found. Expected vendored files under $vendor_lib_dir/ or an installed system package." >&2
      exit 1
    fi

    if [ -f "$vendor_inc_dir/SDL3_image/SDL_image.h" ] && { [ -f "$vendor_lib_dir/libSDL3_image.so.0" ] || [ -f "$vendor_lib_dir/libSDL3_image.dylib" ]; }; then
      sdl3_image_cflags="-I$vendor_inc_dir"
      if [ -f "$vendor_lib_dir/libSDL3_image.so.0" ]; then
        sdl3_image_libs="$vendor_lib_dir/libSDL3_image.so.0"
      else
        sdl3_image_libs="$vendor_lib_dir/libSDL3_image.dylib"
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
      echo "SDL3_image not found. Expected vendored files under $vendor_lib_dir/ or an installed system package." >&2
      exit 1
    fi
  fi

  compiler="clang++"
  common="-std=c++20 $warning_flags -I$src_dir $webgpu_cflags"
  if [ "${time_trace:-}" = "1" ]; then
    common="$common -ftime-trace"
  fi
  host_link=""
  dll_link="$sdl3_image_libs $sdl3_libs $vendor_rpath"
  host_exe="$bin_dir/game_host"

  if [ "$platform" = "macos" ]; then
    common="$common -DSDL_PLATFORM_MACOS"
    host_link="$webgpu_libs $sdl3_image_libs $sdl3_libs $vendor_rpath -framework Cocoa -framework IOKit -framework CoreVideo -lpthread"
    dll_ext=".dylib"
    dll_pch_extra="-fPIC"
    dll_compile_extra="-fPIC -shared"
  elif [ "$platform" = "linux" ]; then
    common="$common -DSDL_PLATFORM_LINUX"
    host_link="$webgpu_libs $sdl3_image_libs $sdl3_libs $vendor_rpath -lpthread"
    dll_ext=".so"
    dll_pch_extra="-fPIC"
    dll_compile_extra="-fPIC -shared"
  else
    common="$common -DSDL_PLATFORM_WIN32 -D_CRT_SECURE_NO_WARNINGS -fuse-ld=lld"
    host_link="$webgpu_libs $sdl3_image_libs $sdl3_libs -luser32 -lgdi32 -lshell32"
    host_exe="$bin_dir/game_host.exe"
    dll_ext=".dll"
    dll_pch_extra=""
    dll_compile_extra="-shared"
  fi

  if [ "${debug:-}" = "1" ]; then
    compile="$compiler -g -O0 $common $sdl3_cflags $sdl3_image_cflags"
    build_config="debug"
  fi
  if [ "${release:-}" = "1" ]; then
    compile="$compiler -O2 -DNDEBUG $common $sdl3_cflags $sdl3_image_cflags"
    build_config="release"
  fi

  mkdir -p "$bin_dir"

  host_compile="$compile"
  dll_pch_compile="$compile $dll_pch_extra"
  dll_compile="$compile $dll_compile_extra"

  pch_dir="$bin_dir/pch"
  pch_header="$src_dir/pch/native_desktop.h"
  host_pch_file="$pch_dir/native-$platform-$host_arch-$build_config-host.pch"
  host_pch_signature_file="$host_pch_file.flags"
  dll_pch_file="$pch_dir/native-$platform-$host_arch-$build_config-dll.pch"
  dll_pch_signature_file="$dll_pch_file.flags"

  mkdir -p "$pch_dir"

  if [ "${game:-}" = "1" ]; then
    ensure_native_pch "$host_compile" "$pch_header" "$host_pch_file" "$host_pch_signature_file"
    host_compile="$host_compile -Winvalid-pch -include-pch $host_pch_file"
  fi

  ensure_native_pch "$dll_pch_compile" "$pch_header" "$dll_pch_file" "$dll_pch_signature_file"
  dll_compile="$dll_compile -Winvalid-pch -include-pch $dll_pch_file"

  if [ "${game:-}" = "1" ] && [ "${install:-}" = "1" ]; then
    if [ -d "$root_dir/assets" ]; then
      mkdir -p "$bin_dir/assets"
      cp -ru "$root_dir/assets/." "$bin_dir/assets/"
    fi

    if [ "$platform" = "win32" ]; then
      for f in "$webgpu_runtime" "$vendor_lib_dir"/SDL3.dll "$vendor_lib_dir"/SDL3_image.dll; do
        [ -f "$f" ] && copy_if_needed "$f" "$bin_dir" 2>/dev/null || true
      done
    else
      for f in "$vendor_lib_dir"/libSDL3*.so* "$vendor_lib_dir"/libSDL3*.dylib* "$webgpu_runtime" "$vendor_lib_dir"/libwgpu_native.so.* "$vendor_lib_dir"/libwgpu_native.dylib.*; do
        [ -f "$f" ] && copy_if_needed "$f" "$bin_dir" 2>/dev/null || true
      done
    fi
  fi

  didbuild=1

  build_failed=0
  game_dll_path="$bin_dir/game_code$dll_ext"

  if [ "${game:-}" = "1" ]; then
    echo "Building game host executable..."
    $host_compile "$src_dir/app/game_main.cpp" $host_link -o "$host_exe" &
    host_pid=$!
  fi

  echo "Building game shared library..."
  $dll_compile "$src_dir/game/game_dll_main.cpp" $webgpu_cflags $sdl3_cflags $sdl3_image_cflags $dll_link -o "$game_dll_path" &
  dll_pid=$!

  if [ "${game:-}" = "1" ]; then
    if wait "$host_pid"; then
      echo "Built $host_exe"
    else
      build_failed=1
    fi
  fi

  if wait "$dll_pid"; then
    echo "Built $game_dll_path"
  else
    build_failed=1
  fi

  if [ "$build_failed" -ne 0 ]; then
    exit "$build_failed"
  fi
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

  web_common="-std=c++20 $warning_flags -I$src_dir"
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
  echo "[WARNING] no valid build target. usage: ./build.sh [game|dll] [web|web_run|run] [debug|release] [install]" >&2
  exit 1
fi
