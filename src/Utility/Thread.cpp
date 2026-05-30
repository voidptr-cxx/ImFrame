/**
 * @file     Thread.cpp
 * @brief    Platform implementation of Thread naming, priority, and CPU affinity
 *
 * @internal
 * Platform guards:
 *   _WIN32         → SetThreadDescription, SetThreadPriority, SetThreadAffinityMask
 *   __APPLE__      → pthread_setname_np (no thread arg), thread_policy_set
 *   (else / Linux) → pthread_setname_np with self, sched_setscheduler,
 *                    pthread_setaffinity_np
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/Thread.hpp"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
    #include <mach/thread_policy.h>
    #include <pthread.h>
#else
    #include <pthread.h>
    #include <sched.h>
#endif

namespace ImFrame::Utility {

// ─── Accessors ────────────────────────────────────────────────────────────────

const std::string& Thread::Name() const noexcept {
    return _name;
}

std::thread::id Thread::Id() const noexcept {
    return _thread.get_id();
}

bool Thread::IsRunning() const noexcept {
    return _running.load(std::memory_order_acquire);
}

void Thread::RequestStop() noexcept {
    _thread.request_stop();
}

// ─── Platform: Windows ───────────────────────────────────────────────────────

#ifdef _WIN32

void Thread::ApplyCurrentThreadName(const std::string& name) {
    // Convert UTF-8 name to wide string for SetThreadDescription.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
    if (wlen <= 0)
        return;
    std::wstring wname(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wname.data(), wlen);
    SetThreadDescription(GetCurrentThread(), wname.c_str());
}

void Thread::SetPriority(Priority priority) {
    int win32Priority = THREAD_PRIORITY_NORMAL;
    switch (priority) {
        case Priority::Low:      win32Priority = THREAD_PRIORITY_BELOW_NORMAL;   break;
        case Priority::Normal:   win32Priority = THREAD_PRIORITY_NORMAL;         break;
        case Priority::High:     win32Priority = THREAD_PRIORITY_ABOVE_NORMAL;   break;
        case Priority::Realtime:
            // TODO(voidptr-cxx, Phase 6): Logger::Warn about RT thread starvation risk.
            win32Priority = THREAD_PRIORITY_TIME_CRITICAL;
            break;
    }
    SetThreadPriority(_thread.native_handle(), win32Priority);
}

void Thread::SetAffinity(uint32_t coreMask) {
    SetThreadAffinityMask(_thread.native_handle(),
                          static_cast<DWORD_PTR>(coreMask));
}

// ─── Platform: macOS ─────────────────────────────────────────────────────────

#elif defined(__APPLE__)

void Thread::ApplyCurrentThreadName(const std::string& name) {
    // On macOS pthread_setname_np applies to the *calling* thread — no handle arg.
    pthread_setname_np(name.c_str());
}

void Thread::SetPriority(Priority priority) {
    // Map ImFrame priority to a 0–63 Mach scheduling quantum percentage.
    thread_extended_policy_data_t extPolicy{};
    thread_precedence_policy_data_t precPolicy{};

    switch (priority) {
        case Priority::Low:
            extPolicy.timeshare   = TRUE;
            precPolicy.importance = 0;
            break;
        case Priority::Normal:
            extPolicy.timeshare   = TRUE;
            precPolicy.importance = 31;
            break;
        case Priority::High:
            extPolicy.timeshare   = TRUE;
            precPolicy.importance = 63;
            break;
        case Priority::Realtime:
            // TODO(voidptr-cxx, Phase 6): Logger::Warn about RT thread starvation risk.
            extPolicy.timeshare   = FALSE;
            precPolicy.importance = 63;
            break;
    }

    mach_port_t port = pthread_mach_thread_np(_thread.native_handle());
    thread_policy_set(port, THREAD_EXTENDED_POLICY,
                      reinterpret_cast<thread_policy_t>(&extPolicy),
                      THREAD_EXTENDED_POLICY_COUNT);
    thread_policy_set(port, THREAD_PRECEDENCE_POLICY,
                      reinterpret_cast<thread_policy_t>(&precPolicy),
                      THREAD_PRECEDENCE_POLICY_COUNT);
}

void Thread::SetAffinity(uint32_t coreMask) {
    // macOS does not support pinning via pthread_setaffinity_np.
    // Best-effort: use THREAD_AFFINITY_POLICY with the lowest set bit as the tag.
    thread_affinity_policy_data_t policy{};
    policy.affinity_tag = static_cast<integer_t>(__builtin_ctz(coreMask) + 1);
    mach_port_t port = pthread_mach_thread_np(_thread.native_handle());
    thread_policy_set(port, THREAD_AFFINITY_POLICY,
                      reinterpret_cast<thread_policy_t>(&policy),
                      THREAD_AFFINITY_POLICY_COUNT);
}

// ─── Platform: Linux / POSIX ─────────────────────────────────────────────────

#else

void Thread::ApplyCurrentThreadName(const std::string& name) {
    // Linux kernel silently truncates names longer than 15 characters.
    pthread_setname_np(pthread_self(), name.c_str());
}

void Thread::SetPriority(Priority priority) {
    int policy    = SCHED_OTHER;
    int schedPrio = 0;

    if (priority == Priority::Realtime) {
        // TODO(voidptr-cxx, Phase 6): Logger::Warn about RT thread starvation risk.
        policy    = SCHED_FIFO;
        schedPrio = 1;
    }
    // SCHED_OTHER ignores the priority field (always 0); nice values would require
    // setpriority() which is process-wide, so we leave Low/High as SCHED_OTHER.

    sched_param param{};
    param.sched_priority = schedPrio;
    pthread_setschedparam(_thread.native_handle(), policy, &param);
}

void Thread::SetAffinity(uint32_t coreMask) {
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    for (int i = 0; i < 32; ++i) {
        if (coreMask & (1u << static_cast<unsigned>(i)))
            CPU_SET(i, &cpuSet);
    }
    pthread_setaffinity_np(_thread.native_handle(), sizeof(cpu_set_t), &cpuSet);
}

#endif

} // namespace ImFrame::Utility
