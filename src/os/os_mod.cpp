#include "os/os_mod.h"

#include "os/os_file.cpp"

#if OS_EMSCRIPTEN
#include "os/os_memory_emscripten.cpp"
#elif OS_WINDOWS
#include "os/os_memory_win32.cpp"
#else
#include "os/os_memory_posix.cpp"
#endif
