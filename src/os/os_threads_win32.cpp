#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "base/base_threads.h"

#if OS_WINDOWS

internal CRITICAL_SECTION* thread_mutex_handle(ThreadMutex* mutex) {
    return (CRITICAL_SECTION*)mutex->storage;
}

internal CONDITION_VARIABLE* thread_condition_variable_handle(
    ThreadConditionVariable* condition_variable
) {
    return (CONDITION_VARIABLE*)condition_variable->storage;
}

static_assert(
    sizeof(ThreadMutex) >= sizeof(CRITICAL_SECTION),
    "ThreadMutex storage is too small for CRITICAL_SECTION"
);
static_assert(
    alignof(ThreadMutex) >= alignof(CRITICAL_SECTION),
    "ThreadMutex alignment is too small for CRITICAL_SECTION"
);
static_assert(
    sizeof(ThreadConditionVariable) >= sizeof(CONDITION_VARIABLE),
    "ThreadConditionVariable storage is too small for CONDITION_VARIABLE"
);
static_assert(
    alignof(ThreadConditionVariable) >= alignof(CONDITION_VARIABLE),
    "ThreadConditionVariable alignment is too small for CONDITION_VARIABLE"
);

bool init_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    InitializeCriticalSection(thread_mutex_handle(mutex));
    return true;
}

void destroy_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    DeleteCriticalSection(thread_mutex_handle(mutex));
}

void lock_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    EnterCriticalSection(thread_mutex_handle(mutex));
}

void unlock_thread_mutex(ThreadMutex* mutex) {
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    LeaveCriticalSection(thread_mutex_handle(mutex));
}

bool init_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    InitializeConditionVariable(
        thread_condition_variable_handle(condition_variable)
    );
    return true;
}

void destroy_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
}

void wake_all_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    WakeAllConditionVariable(
        thread_condition_variable_handle(condition_variable)
    );
}

void wait_thread_condition_variable(
    ThreadConditionVariable* condition_variable,
    ThreadMutex* mutex
) {
    ASSERT(
        condition_variable != nullptr,
        "Thread condition variable must not be null!"
    );
    ASSERT(mutex != nullptr, "Thread mutex must not be null!");
    SleepConditionVariableCS(
        thread_condition_variable_handle(condition_variable),
        thread_mutex_handle(mutex),
        INFINITE
    );
}

bool create_thread(Thread* thread, ThreadProc* proc, void* data) {
    ASSERT(thread != nullptr, "Thread must not be null!");
    ASSERT(proc != nullptr, "Thread proc must not be null!");

    thread->handle = CreateThread(nullptr, 0, proc, data, 0, nullptr);
    return thread->handle != nullptr;
}

void join_thread(Thread* thread) {
    ASSERT(thread != nullptr, "Thread must not be null!");
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = nullptr;
}

u32 get_logical_processor_count(void) {
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    if(system_info.dwNumberOfProcessors == 0) {
        return 1;
    }

    return (u32)system_info.dwNumberOfProcessors;
}

#endif
