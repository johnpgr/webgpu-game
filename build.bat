@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Unpack Arguments --------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1" set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]

set root_dir=%cd%
set vendor_dir=%root_dir%\vendor

if /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set host_arch=x64
) else if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set host_arch=arm64
) else (
    set host_arch=x86
)

set vendor_inc_dir=%vendor_dir%\include
set vendor_lib_dir=%vendor_dir%\lib\win32-!host_arch!
set vendor_emsdk_dir=%vendor_dir%\emsdk

if "%vendor%"=="1" (
    echo vendor downloads have been removed.
    echo Place native Windows headers under %vendor_inc_dir%\, for example %vendor_inc_dir%\SDL3\.
    echo Place native Windows libraries under %vendor_lib_dir%\.
    echo Keep the Emscripten SDK under %vendor_dir%\emsdk\.
    exit /b 1
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
    if not exist "!vendor_inc_dir!\webgpu\webgpu.h" (
        echo WebGPU headers not found at !vendor_inc_dir!\webgpu\webgpu.h
        echo Place native Windows headers under !vendor_inc_dir!\
        exit /b 1
    )

    if not exist "!vendor_inc_dir!\SDL3\SDL.h" (
        echo SDL3 headers not found at !vendor_inc_dir!\SDL3\SDL.h
        echo Place native Windows headers under !vendor_inc_dir!\
        exit /b 1
    )

    if not exist "!vendor_inc_dir!\SDL3_image\SDL_image.h" (
        echo SDL3_image headers not found at !vendor_inc_dir!\SDL3_image\SDL_image.h
        echo Place native Windows headers under !vendor_inc_dir!\
        exit /b 1
    )

    if not exist "!vendor_lib_dir!\wgpu_native.lib" (
        echo WebGPU !host_arch! libraries not found at !vendor_lib_dir!\wgpu_native.lib
        echo Place native Windows libraries under !vendor_lib_dir!\
        exit /b 1
    )

    if not exist "!vendor_lib_dir!\SDL3.lib" (
        echo SDL3 !host_arch! libraries not found at !vendor_lib_dir!\SDL3.lib
        echo Place native Windows libraries under !vendor_lib_dir!\
        exit /b 1
    )

    if not exist "!vendor_lib_dir!\SDL3_image.lib" (
        echo SDL3_image !host_arch! libraries not found at !vendor_lib_dir!\SDL3_image.lib
        echo Place native Windows libraries under !vendor_lib_dir!\
        exit /b 1
    )

    call :ensure_not_lfs_pointer "!vendor_lib_dir!\wgpu_native.lib" || exit /b 1
    call :ensure_not_lfs_pointer "!vendor_lib_dir!\SDL3.lib" || exit /b 1
    call :ensure_not_lfs_pointer "!vendor_lib_dir!\SDL3_image.lib" || exit /b 1

    echo [win32 !host_arch!]
    set common=/std:c++20 /nologo /W4 /WX /wd4505 /wd4127 /wd4201 /wd4204 /wd4996 /I"!src_dir!" /I"!vendor_inc_dir!" /DSDL_PLATFORM_WIN32
    set clang_common=/clang:-Wno-c99-designator /clang:-fuse-ld=lld
    if "%debug%"=="1"   set compile=clang-cl !common! !clang_common! /Od /Z7
    if "%release%"=="1" set compile=clang-cl !common! !clang_common! /O2 /DNDEBUG

    set host_libs=/LIBPATH:"!vendor_lib_dir!" wgpu_native.lib SDL3_image.lib SDL3.lib user32.lib gdi32.lib shell32.lib
    set dll_libs=/LIBPATH:"!vendor_lib_dir!" SDL3_image.lib SDL3.lib

    if "%game%"=="1" (
        if exist "%root_dir%\assets" (
            xcopy /s /y "%root_dir%\assets" "%bin_dir%\assets\" >nul 2>&1
        )

        if exist "!vendor_lib_dir!\wgpu_native.dll" copy /y "!vendor_lib_dir!\wgpu_native.dll" "!bin_dir!\" >nul 2>&1
        if exist "!vendor_lib_dir!\SDL3.dll" copy /y "!vendor_lib_dir!\SDL3.dll" "!bin_dir!\" >nul 2>&1
        if exist "!vendor_lib_dir!\SDL3_image.dll" copy /y "!vendor_lib_dir!\SDL3_image.dll" "!bin_dir!\" >nul 2>&1
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

    set emsdk_dir=%vendor_emsdk_dir%
    set emsdk_env=!emsdk_dir!\emsdk_env.bat
    if exist "!emsdk_env!" call "!emsdk_env!" >nul

    where emcc >nul 2>&1 || (
        echo emcc not found. Expected emsdk at %vendor_dir%\win32-x64\emsdk\ or on PATH.
        exit /b 1
    )

    if not exist "%web_dir%" mkdir "%web_dir%"

    set didbuild=1
    if "%debug%"=="1" (
        call emcc -g -O0 -std=c++11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator -I"%src_dir%" -sUSE_SDL=3 --use-port=emdawnwebgpu -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=167772160 -sASYNCIFY=1 -DNDEBUG -DSDL_PLATFORM_EMSCRIPTEN -sASSERTIONS=2 "%src_dir%\app\game_main.cpp" --shell-file "%src_dir%\app\game_main.html" --preload-file "%root_dir%\assets@/assets" --use-preload-plugins -sEXPORTED_FUNCTIONS=['_main','_malloc','_free'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] -o "%web_dir%\game.html" || exit /b 1
    ) else (
        call emcc -O2 -std=c++11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Wno-c99-designator -I"%src_dir%" -sUSE_SDL=3 --use-port=emdawnwebgpu -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=167772160 -sASYNCIFY=1 -DNDEBUG -DSDL_PLATFORM_EMSCRIPTEN "%src_dir%\app\game_main.cpp" --shell-file "%src_dir%\app\game_main.html" --preload-file "%root_dir%\assets@/assets" --use-preload-plugins -sEXPORTED_FUNCTIONS=['_main','_malloc','_free'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] -o "%web_dir%\game.html" || exit /b 1
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
        echo emrun not found. Expected emsdk at %vendor_dir%\win32-x64\emsdk\ or on PATH.
        exit /b 1
    )

    echo serving at http://localhost:6931/game.html
    call emrun "%web_dir%\game.html"
)

:: --- Warn On No Builds -------------------------------------------------------
if "%didbuild%"=="" if not "%run%"=="1" (
    echo [WARNING] no valid build target. usage: build [game^|dll] [web^|web_run] [run] [debug^|release]
    exit /b 1
)

echo Build complete.
goto :eof

:ensure_not_lfs_pointer
set "check_path=%~1"
set "check_first_line="
for /f "usebackq delims=" %%i in ("%~1") do (
    set "check_first_line=%%i"
    goto :ensure_not_lfs_pointer_done
)

:ensure_not_lfs_pointer_done
if "!check_first_line!"=="version https://git-lfs.github.com/spec/v1" (
    echo Git LFS object missing for %~nx1
    echo Run: git lfs pull
    exit /b 1
)
exit /b 0
