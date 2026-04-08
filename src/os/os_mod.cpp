#include "os/os_mod.h"

#if OS_EMSCRIPTEN
#include "os/os_memory_emscripten.cpp"
#include "os/os_threads_emscripten.cpp"
#elif OS_WINDOWS
#include "os/os_memory_win32.cpp"
#include "os/os_threads_win32.cpp"
#else
#include "os/os_memory_posix.cpp"
#include "os/os_threads_posix.cpp"
#endif
