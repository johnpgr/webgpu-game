@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Unpack Arguments --------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]

set root_dir=%cd%
set vendor_dir=%root_dir%\vendor

:: --- Download Vendor Dependencies --------------------------------------------
if "%vendor%"=="1" (
    echo === Downloading vendor dependencies ===

    :: Detect architecture
    if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
        set dl_arch=x64
    ) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
        set dl_arch=arm64
    ) else (
        set dl_arch=x86
    )

    :: --- SDL3 ---
    set SDL3_VERSION=3.4.4
    set SDL3_BASE=https://github.com/libsdl-org/SDL/releases/download/release-!SDL3_VERSION!

    echo [SDL3] Downloading SDL !SDL3_VERSION! for Windows...
    if not exist "%vendor_dir%\sdl3_tmp" mkdir "%vendor_dir%\sdl3_tmp"

    powershell -Command "Invoke-WebRequest -Uri '!SDL3_BASE!/SDL3-devel-!SDL3_VERSION!-VC.zip' -OutFile '%vendor_dir%\sdl3_tmp\sdl3.zip'" || (
        echo [SDL3] Download failed!
        exit /b 1
    )

    echo [SDL3] Extracting...
    powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%vendor_dir%\sdl3_tmp\sdl3.zip', '%vendor_dir%\sdl3_tmp')"

    :: Find extracted directory
    for /d %%d in ("%vendor_dir%\sdl3_tmp\SDL3-*") do set sdl3_extracted=%%d

    if not exist "%vendor_dir%\SDL3" mkdir "%vendor_dir%\SDL3"
    if exist "!sdl3_extracted!\SDL3-!SDL3_VERSION!\include" (
        xcopy /s /y /q "!sdl3_extracted!\SDL3-!SDL3_VERSION!\include" "%vendor_dir%\SDL3\include\" >nul
        xcopy /s /y /q "!sdl3_extracted!\SDL3-!SDL3_VERSION!\lib" "%vendor_dir%\SDL3\lib\" >nul
    ) else if exist "!sdl3_extracted!\include" (
        xcopy /s /y /q "!sdl3_extracted!\include" "%vendor_dir%\SDL3\include\" >nul
        xcopy /s /y /q "!sdl3_extracted!\lib" "%vendor_dir%\SDL3\lib\" >nul
    )

    rmdir /s /q "%vendor_dir%\sdl3_tmp"
    echo [SDL3] Done.

    :: --- SDL3_image ---
    set SDL3_IMAGE_VERSION=3.4.2
    set SDL3_IMAGE_BASE=https://github.com/libsdl-org/SDL_image/releases/download/release-!SDL3_IMAGE_VERSION!

    echo [SDL3_image] Downloading SDL_image !SDL3_IMAGE_VERSION! for Windows...
    if not exist "%vendor_dir%\sdl3_image_tmp" mkdir "%vendor_dir%\sdl3_image_tmp"

    powershell -Command "Invoke-WebRequest -Uri '!SDL3_IMAGE_BASE!/SDL3_image-devel-!SDL3_IMAGE_VERSION!-VC.zip' -OutFile '%vendor_dir%\sdl3_image_tmp\sdl3_image.zip'" || (
        echo [SDL3_image] Download failed!
        exit /b 1
    )

    echo [SDL3_image] Extracting...
    powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%vendor_dir%\sdl3_image_tmp\sdl3_image.zip', '%vendor_dir%\sdl3_image_tmp')"

    :: Find extracted directory
    for /d %%d in ("%vendor_dir%\sdl3_image_tmp\SDL3_image-*") do set sdl3_image_extracted=%%d

    if not exist "%vendor_dir%\SDL3_image" mkdir "%vendor_dir%\SDL3_image"
    if exist "!sdl3_image_extracted!\SDL3_image-!SDL3_IMAGE_VERSION!\include" (
        xcopy /s /y /q "!sdl3_image_extracted!\SDL3_image-!SDL3_IMAGE_VERSION!\include" "%vendor_dir%\SDL3_image\include\" >nul
        xcopy /s /y /q "!sdl3_image_extracted!\SDL3_image-!SDL3_IMAGE_VERSION!\lib" "%vendor_dir%\SDL3_image\lib\" >nul
    ) else if exist "!sdl3_image_extracted!\include" (
        xcopy /s /y /q "!sdl3_image_extracted!\include" "%vendor_dir%\SDL3_image\include\" >nul
        xcopy /s /y /q "!sdl3_image_extracted!\lib" "%vendor_dir%\SDL3_image\lib\" >nul
    )

    rmdir /s /q "%vendor_dir%\sdl3_image_tmp"
    echo [SDL3_image] Done.

    :: --- wgpu-native (WebGPU C API) ---
    set WGPU_VERSION=27.0.4.0
    set WGPU_BASE=https://github.com/gfx-rs/wgpu-native/releases/download/v!WGPU_VERSION!

    if "!dl_arch!"=="x64" (
        set wgpu_artifact=wgpu-windows-x86_64-msvc-release.zip
    ) else if "!dl_arch!"=="arm64" (
        set wgpu_artifact=wgpu-windows-aarch64-msvc-release.zip
    ) else (
        set wgpu_artifact=wgpu-windows-i686-msvc-release.zip
    )

    echo [wgpu-native] Downloading wgpu-native v!WGPU_VERSION!...
    if not exist "%vendor_dir%\wgpu_tmp" mkdir "%vendor_dir%\wgpu_tmp"

    powershell -Command "Invoke-WebRequest -Uri '!WGPU_BASE!/!wgpu_artifact!' -OutFile '%vendor_dir%\wgpu_tmp\wgpu.zip'" || (
        echo [wgpu-native] Download failed!
        exit /b 1
    )

    echo [wgpu-native] Extracting...
    if not exist "%vendor_dir%\wgpu_tmp\extracted" mkdir "%vendor_dir%\wgpu_tmp\extracted"
    powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%vendor_dir%\wgpu_tmp\wgpu.zip', '%vendor_dir%\wgpu_tmp\extracted')"

    :: Find extracted directory - wgpu extracts directly without a wrapper dir
    set wgpu_inner=%vendor_dir%\wgpu_tmp\extracted
    if not exist "!wgpu_inner!\include\webgpu\webgpu.h" (
        for /d %%d in ("!wgpu_inner!\*") do (
            if exist "%%d\include\webgpu\webgpu.h" set wgpu_inner=%%d
        )
    )

    if not exist "%vendor_dir%\webgpu\include\webgpu" mkdir "%vendor_dir%\webgpu\include\webgpu"
    if not exist "%vendor_dir%\webgpu\lib" mkdir "%vendor_dir%\webgpu\lib"

    :: Copy headers
    if exist "!wgpu_inner!\include\webgpu" (
        xcopy /y "!wgpu_inner!\include\webgpu\*" "%vendor_dir%\webgpu\include\webgpu\" >nul
    ) else if exist "!wgpu_inner!\webgpu.h" (
        copy /y "!wgpu_inner!\webgpu.h" "%vendor_dir%\webgpu\include\webgpu\" >nul
    )

    :: Copy libraries
    if exist "!wgpu_inner!\lib" (
        copy /y "!wgpu_inner!\lib\*" "%vendor_dir%\webgpu\lib\" >nul 2>&1
    )
    for %%e in (lib dll pdb) do (
        if exist "!wgpu_inner!\*.%%e" copy /y "!wgpu_inner!\*.%%e" "%vendor_dir%\webgpu\lib\" >nul 2>&1
    )

    rmdir /s /q "%vendor_dir%\wgpu_tmp"
    echo [wgpu-native] Done.

    :: --- Emscripten SDK ---
    set emsdk_dir=%vendor_dir%\emsdk
    if exist "%emsdk_dir%\upstream\emscripten\emcc.bat" (
        echo [emscripten] Already installed, skipping...
    ) else (
        echo [emscripten] Installing Emscripten SDK...
        if not exist "%emsdk_dir%" (
            git clone https://github.com/emscripten-core/emsdk.git "%emsdk_dir%" || (
                echo [emscripten] Clone failed!
                exit /b 1
            )
        ) else (
            pushd "%emsdk_dir%"
            git pull || (
                popd
                echo [emscripten] Update failed!
                exit /b 1
            )
            popd
        )

        pushd "%emsdk_dir%"
        call emsdk.bat install latest || (
            popd
            echo [emscripten] Install failed!
            exit /b 1
        )
        call emsdk.bat activate latest || (
            popd
            echo [emscripten] Activate failed!
            exit /b 1
        )
        popd
        echo [emscripten] Done.
    )

    echo === Vendor dependencies installed to %vendor_dir% ===
    dir /s /b "%vendor_dir%" | find /c /v "" & echo files
    exit /b 0
)

:: --- Paths -------------------------------------------------------------------
set bin_dir=%root_dir%\bin
set web_dir=%bin_dir%\web
set src_dir=%root_dir%\src
if not exist "%bin_dir%" mkdir "%bin_dir%"

:: --- Build Target Selection --------------------------------------------------
if "%web_run%"=="1" (
    set web=1
    set run=1
)

if "%run%"=="1" if "%game%"=="" if "%web%"=="" set web=1
if "%game%"=="" if "%web%"=="" if "%dll%"=="" set game=1

:: --- Build Native Game or DLL (Win32) ----------------------------------------
set build_native=0
if "%game%"=="1" set build_native=1
if "%dll%"=="1" set build_native=1

if "!build_native!"=="1" (
    set webgpu_dir=%root_dir%\vendor\webgpu
    set sdl3_dir=%root_dir%\vendor\SDL3
    set sdl3_image_dir=%root_dir%\vendor\SDL3_image

    if not exist "%webgpu_dir%\include\webgpu\webgpu.h" (
        echo WebGPU headers not found at %webgpu_dir%\include\webgpu\webgpu.h
        echo Run: build vendor   to download dependencies
        exit /b 1
    )

    if not exist "%sdl3_dir%\include\SDL3\SDL.h" (
        echo SDL3 not found at %sdl3_dir%\include\SDL3\SDL.h
        echo Run: build vendor   to download dependencies
        exit /b 1
    )

    if not exist "%sdl3_image_dir%\include\SDL3_image\SDL_image.h" (
        echo SDL3_image not found at %sdl3_image_dir%\include\SDL3_image\SDL_image.h
        echo Run: build vendor   to download dependencies
        exit /b 1
    )

    echo [win32]
    set common=/std:c++20 /nologo /W4 /WX /wd4505 /wd4127 /wd4201 /wd4204 /wd4996 /I"%src_dir%" /I"%webgpu_dir%\include" /I"%sdl3_dir%\include" /I"%sdl3_image_dir%\include" /DSDL_PLATFORM_WIN32
    if "%debug%"=="1"   set compile=cl !common! /Od /Zi
    if "%release%"=="1" set compile=cl !common! /O2 /DNDEBUG

    set host_libs=/LIBPATH:"%webgpu_dir%\lib" /LIBPATH:"%sdl3_dir%\lib\x64" /LIBPATH:"%sdl3_image_dir%\lib\x64" wgpu_native.lib SDL3_image.lib SDL3.lib user32.lib gdi32.lib shell32.lib
    set dll_libs=/LIBPATH:"%sdl3_dir%\lib\x64" /LIBPATH:"%sdl3_image_dir%\lib\x64" SDL3_image.lib SDL3.lib

    if "%game%"=="1" (
        if exist "%root_dir%\assets" (
            xcopy /s /y "%root_dir%\assets" "%bin_dir%\assets\" >nul 2>&1
        )

        if exist "%webgpu_dir%\lib\wgpu_native.dll" copy /y "%webgpu_dir%\lib\wgpu_native.dll" "%bin_dir%\" >nul 2>&1
        if exist "%sdl3_dir%\lib\x64\SDL3.dll" copy /y "%sdl3_dir%\lib\x64\SDL3.dll" "%bin_dir%\" >nul 2>&1
        if exist "%sdl3_image_dir%\lib\x64\SDL3_image.dll" copy /y "%sdl3_image_dir%\lib\x64\SDL3_image.dll" "%bin_dir%\" >nul 2>&1
    )

    pushd "%bin_dir%"
    set didbuild=1

    if "%game%"=="1" (
        echo Building game host executable...
        !compile! "%src_dir%\app\game_main.cpp" /link !host_libs! /out:"%bin_dir%\game_host.exe" || (
            popd
            exit /b 1
        )
        echo Built %bin_dir%\game_host.exe
    )

    echo Building game DLL...
    !compile! /LD "%src_dir%\game\game_dll_main.cpp" /link !dll_libs! /out:"%bin_dir%\game_code.dll" || (
        popd
        exit /b 1
    )
    echo Built %bin_dir%\game_code.dll
    popd
)

:: --- Build Web Game (Emscripten) ---------------------------------------------
if "%web%"=="1" (
    echo [web]

    set emsdk_dir=%vendor_dir%\emsdk
    set emsdk_env=%emsdk_dir%\emsdk_env.bat
    if exist "%emsdk_env%" call "%emsdk_env%" >nul

    where emcc >nul 2>&1 || (
        echo emcc not found. Run: build vendor
        exit /b 1
    )

    if not exist "%web_dir%" mkdir "%web_dir%"

    set didbuild=1
    if "%debug%"=="1" (
        emcc -g -O0 -std=c++11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator -I"%src_dir%" -sUSE_SDL=3 --use-port=emdawnwebgpu -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=167772160 -sASYNCIFY=1 -DNDEBUG -DSDL_PLATFORM_EMSCRIPTEN -sASSERTIONS=2 "%src_dir%\app\game_main.cpp" --shell-file "%src_dir%\app\game_main.html" --preload-file "%root_dir%\assets@/assets" --use-preload-plugins -sEXPORTED_FUNCTIONS=['_main','_malloc','_free'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] -o "%web_dir%\game.html" || exit /b 1
    ) else (
        emcc -O2 -std=c++11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator -I"%src_dir%" -sUSE_SDL=3 --use-port=emdawnwebgpu -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=167772160 -sASYNCIFY=1 -DNDEBUG -DSDL_PLATFORM_EMSCRIPTEN "%src_dir%\app\game_main.cpp" --shell-file "%src_dir%\app\game_main.html" --preload-file "%root_dir%\assets@/assets" --use-preload-plugins -sEXPORTED_FUNCTIONS=['_main','_malloc','_free'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] -o "%web_dir%\game.html" || exit /b 1
    )

    echo Built %web_dir%\game.html
)

:: --- Run Web Build -----------------------------------------------------------
if "%run%"=="1" (
    if not exist "%web_dir%\game.html" (
        echo game.html not found. Build first with: build web
        exit /b 1
    )

    where emrun >nul 2>&1 || (
        echo emrun not found. Run: build vendor
        exit /b 1
    )

    echo serving at http://localhost:6931/game.html
    emrun "%web_dir%\game.html"
)

:: --- Warn On No Builds -------------------------------------------------------
if "%didbuild%"=="" if not "%run%"=="1" (
    echo [WARNING] no valid build target. usage: build [vendor] [game^|dll] [web^|web_run] [run] [debug^|release]
    exit /b 1
)

echo Build complete.