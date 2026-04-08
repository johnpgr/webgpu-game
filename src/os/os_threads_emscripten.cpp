// Emscripten thread stubs - single-threaded
#include "base/base_threads.h"

bool init_thread_mutex(ThreadMutex* mutex) {
    (void)mutex;
    return true;
}

void destroy_thread_mutex(ThreadMutex* mutex) {
    (void)mutex;
}

void lock_thread_mutex(ThreadMutex* mutex) {
    (void)mutex;
}

void unlock_thread_mutex(ThreadMutex* mutex) {
    (void)mutex;
}

bool init_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    (void)condition_variable;
    return true;
}

void destroy_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    (void)condition_variable;
}

void wake_all_thread_condition_variable(
    ThreadConditionVariable* condition_variable
) {
    (void)condition_variable;
}

void wait_thread_condition_variable(
    ThreadConditionVariable* condition_variable,
    ThreadMutex* mutex
) {
    (void)condition_variable;
    (void)mutex;
}

bool create_thread(Thread* thread, ThreadProc* proc, void* data) {
    (void)thread;
    (void)proc;
    (void)data;
    return false;
}

void join_thread(Thread* thread) {
    (void)thread;
}

u32 get_logical_processor_count(void) {
    return 1;
}
