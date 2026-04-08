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

    echo === Vendor dependencies installed to %vendor_dir% ===
    dir /s /b "%vendor_dir%" | find /c /v "" & echo files
    exit /b 0
)

:: --- Paths -------------------------------------------------------------------
set bin_dir=%root_dir%\bin
set src_dir=%root_dir%\src
set bin_dir_fwd=%bin_dir:\=/%
if not exist "%bin_dir%" mkdir "%bin_dir%"

:: --- Find WebGPU + SDL3 ------------------------------------------------------
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

:: --- Compile/Link Line Definitions -------------------------------------------
set common=/std:c++20 /nologo /W4 /WX /wd4505 /wd4127 /wd4201 /wd4996 /I"%src_dir%" /I"%webgpu_dir%\include" /I"%sdl3_dir%\include" /I"%sdl3_image_dir%\include" /DSDL_PLATFORM_WIN32
if "%debug%"=="1"   set compile=cl %common% /Od /Zi
if "%release%"=="1" set compile=cl %common% /O2 /DNDEBUG

set host_libs=/LIBPATH:"%webgpu_dir%\lib" /LIBPATH:"%sdl3_dir%\lib\x64" /LIBPATH:"%sdl3_image_dir%\lib\x64" wgpu_native.lib SDL3_image.lib SDL3.lib user32.lib gdi32.lib shell32.lib
set dll_libs=/LIBPATH:"%sdl3_dir%\lib\x64" /LIBPATH:"%sdl3_image_dir%\lib\x64" SDL3_image.lib SDL3.lib

:: --- Copy Assets -------------------------------------------------------------
if exist "%root_dir%\assets" (
    xcopy /s /y "%root_dir%\assets" "%bin_dir%\assets\" >nul 2>&1
)

:: --- Copy DLLs to bin --------------------------------------------------------
if exist "%webgpu_dir%\lib\wgpu_native.dll" copy /y "%webgpu_dir%\lib\wgpu_native.dll" "%bin_dir%\" >nul 2>&1
if exist "%sdl3_dir%\lib\x64\SDL3.dll" copy /y "%sdl3_dir%\lib\x64\SDL3.dll" "%bin_dir%\" >nul 2>&1
if exist "%sdl3_image_dir%\lib\x64\SDL3_image.dll" copy /y "%sdl3_image_dir%\lib\x64\SDL3_image.dll" "%bin_dir%\" >nul 2>&1

:: --- Build Targets -----------------------------------------------------------
if "%~1"==""                      set game=1
if "%~1"=="debug"   if "%~2"=="" set game=1
if "%~1"=="release" if "%~2"=="" set game=1

pushd "%bin_dir%"
if "%game%"=="1" (
    set didbuild=1
    
    echo Building game host executable...
    %compile% "%src_dir%\app\game_main.cpp" /link %host_libs% /out:"%bin_dir%\game_host.exe" || exit /b 1
    echo Built %bin_dir%\game_host.exe
    
    echo Building game DLL...
    %compile% /LD "%src_dir%\game\game_dll_main.cpp" /link %dll_libs% /out:"%bin_dir%\game_code.dll" || exit /b 1
    echo Built %bin_dir%\game_code.dll
)
popd

:: --- Warn On No Builds -------------------------------------------------------
if not "%didbuild%"=="1" (
    echo [WARNING] no valid build target. usage: build [vendor] [game] [debug^|release]
    exit /b 1
)
echo Build complete.