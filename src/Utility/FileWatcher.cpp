/**
 * @file     FileWatcher.cpp
 * @brief    Cross-platform filesystem change notification implementation
 *
 * @internal
 * Platform selection:
 *   _WIN32        → ReadDirectoryChangesW (directory handle per watch)
 *   __APPLE__     → kqueue (shared kq fd, EVFILT_VNODE per watched dir)
 *   (Linux/other) → inotify (shared inotify fd, one watch descriptor per dir)
 *
 * Shared design for all platforms:
 *   - The platform handle (inotify fd / kq fd) is created at Impl construction.
 *   - Watch() / Unwatch() are called on the owner thread and update _watches
 *     under _watchesMtx. The background thread reads _watches under the same
 *     lock to register new / deregister removed entries.
 *   - Events are pushed to _queue (guarded by _queueMtx).
 *   - Poll() swaps the queue to a local deque and dispatches without holding
 *     any lock.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Utility/FileWatcher.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <chrono>

// ─── Platform includes ────────────────────────────────────────────────────────
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <sys/event.h>
    #include <sys/time.h>
    #include <fcntl.h>
    #include <unistd.h>
#else // Linux / POSIX
    #include <sys/inotify.h>
    #include <sys/select.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace ImFrame::Utility {

// =============================================================================
// WatchEntry — per-watched-path state
// =============================================================================

struct WatchEntry {
    Path        path;
    WatchHandle handle = InvalidWatchHandle;

#ifdef _WIN32
    HANDLE      dirHandle    = INVALID_HANDLE_VALUE;
    HANDLE      changeEvent  = INVALID_HANDLE_VALUE; ///< Manual-reset event for overlapped RDCW
    OVERLAPPED  ovl          = {};                   ///< Overlapped struct — stable in map
    char        buf[32*1024] = {};                   ///< RDCW output buffer
    bool        rdcwPending  = false;
#elif defined(__APPLE__)
    int         fd  = -1;    ///< Open file descriptor for the directory (kqueue udata)
    bool        registered = false;
#else
    int         wd  = -1;    ///< inotify watch descriptor
#endif
};

// =============================================================================
// Impl — common + platform-specific fields
// =============================================================================

struct FileWatcher::Impl {
    // ── Shared queue ──────────────────────────────────────────────────────────
    std::deque<FileEvent>                         _queue;
    std::mutex                                    _queueMtx;

    // ── Watch registry ────────────────────────────────────────────────────────
    std::unordered_map<WatchHandle, WatchEntry>   _watches;
    std::mutex                                    _watchesMtx;
    std::atomic<WatchHandle>                      _nextHandle{1};

    // ── Background thread ─────────────────────────────────────────────────────
    std::jthread                                  _thread;
    std::atomic<bool>                             _running{false};

    // ── Platform stop / wake mechanism ────────────────────────────────────────
#ifdef _WIN32
    HANDLE  _stopEvent  = INVALID_HANDLE_VALUE;
#else
    int     _stopPipe[2]{-1, -1};
#endif

    // ── Platform watcher handle ───────────────────────────────────────────────
#ifdef _WIN32
    // Windows uses per-directory HANDLE stored in WatchEntry::dirHandle.
#elif defined(__APPLE__)
    int     _kq  = -1;      ///< kqueue file descriptor
#else
    int     _inotifyFd = -1; ///< inotify file descriptor
#endif

    // ── Common methods ────────────────────────────────────────────────────────
    Impl();
    ~Impl();
    void Start();
    void Stop();
    void ThreadFunc();
    void PushEvent(FileEvent ev);
    Result<WatchHandle> AddWatch(const Path& path);
    void                RemoveWatch(WatchHandle handle);
};

// ─── PushEvent ────────────────────────────────────────────────────────────────

void FileWatcher::Impl::PushEvent(FileEvent ev)
{
    std::lock_guard lock{_queueMtx};
    _queue.push_back(std::move(ev));
}

// =============================================================================
//  ██╗    ██╗██╗███╗   ██╗██████╗  ██████╗ ██╗    ██╗███████╗
//  ██║    ██║██║████╗  ██║██╔══██╗██╔═══██╗██║    ██║██╔════╝
//  ██║ █╗ ██║██║██╔██╗ ██║██║  ██║██║   ██║██║ █╗ ██║███████╗
//  ██║███╗██║██║██║╚██╗██║██║  ██║██║   ██║██║███╗██║╚════██║
//  ╚███╔███╔╝██║██║ ╚████║██████╔╝╚██████╔╝╚███╔███╔╝███████║
//   ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝╚═════╝  ╚═════╝  ╚══╝╚══╝╚══════╝
// =============================================================================
#ifdef _WIN32

FileWatcher::Impl::Impl()
{
    _stopEvent = CreateEventW(nullptr, /*manualReset=*/TRUE, /*initial=*/FALSE, nullptr);
}

FileWatcher::Impl::~Impl() { Stop(); }

void FileWatcher::Impl::Start()
{
    _running = true;
    _thread  = std::jthread{[this](std::stop_token /*st*/) { ThreadFunc(); }};
}

// ─── Internal helper: issue overlapped RDCW for one entry (must hold lock) ────

namespace {

constexpr DWORD kRdcwFilter =
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
    FILE_NOTIFY_CHANGE_DIR_NAME  | FILE_NOTIFY_CHANGE_CREATION;

bool IssueRdcw(WatchEntry& e)
{
    if (e.dirHandle == INVALID_HANDLE_VALUE || e.changeEvent == INVALID_HANDLE_VALUE) {
        return false;
    }
    ResetEvent(e.changeEvent);
    e.ovl       = {};
    e.ovl.hEvent = e.changeEvent;
    BOOL ok = ReadDirectoryChangesW(
        e.dirHandle,
        e.buf, static_cast<DWORD>(sizeof(e.buf)),
        /*watchSubtree=*/FALSE, kRdcwFilter,
        nullptr, &e.ovl, nullptr);
    e.rdcwPending = (ok == TRUE);
    return e.rdcwPending;
}

void ProcessRdcwCompletion(WatchEntry& e,
                           const std::function<void(FileEvent)>& pushFn)
{
    DWORD bytes = 0;
    if (!GetOverlappedResult(e.dirHandle, &e.ovl, &bytes, FALSE)) {
        e.rdcwPending = false;
        return;
    }
    e.rdcwPending = false;

    if (bytes == 0) return;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* fni = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(e.buf);
    while (true) {
        std::wstring wname{fni->FileName, fni->FileNameLength / sizeof(WCHAR)};
        FileEvent fev;
        fev.ChangedPath = e.path / std::filesystem::path{wname}.string();
        switch (fni->Action) {
            case FILE_ACTION_ADDED:            fev.Type = FileChangeType::Created;  break;
            case FILE_ACTION_REMOVED:          fev.Type = FileChangeType::Deleted;  break;
            case FILE_ACTION_RENAMED_OLD_NAME: fev.Type = FileChangeType::Renamed;  break;
            default:                           fev.Type = FileChangeType::Modified; break;
        }
        pushFn(std::move(fev));
        if (fni->NextEntryOffset == 0) break;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        fni = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
            reinterpret_cast<const char*>(fni) + fni->NextEntryOffset);
    }
}

} // anonymous namespace (Windows-only helpers)

// ─────────────────────────────────────────────────────────────────────────────

void FileWatcher::Impl::Stop()
{
    if (!_running.exchange(false)) return;

    // Signal the thread — it uses WFMO with a 100ms timeout so it will
    // wake within 100ms and see _running == false. std::jthread joins
    // automatically when _thread is destructed after Stop() returns.
    if (_stopEvent != INVALID_HANDLE_VALUE) SetEvent(_stopEvent);

    std::lock_guard lock{_watchesMtx};
    for (auto& [h, e] : _watches) {
        if (e.dirHandle != INVALID_HANDLE_VALUE) {
            CancelIoEx(e.dirHandle, nullptr);
            CloseHandle(e.dirHandle);
            e.dirHandle = INVALID_HANDLE_VALUE;
        }
        if (e.changeEvent != INVALID_HANDLE_VALUE) {
            CloseHandle(e.changeEvent);
            e.changeEvent = INVALID_HANDLE_VALUE;
        }
    }
    _watches.clear();

    if (_stopEvent != INVALID_HANDLE_VALUE) {
        CloseHandle(_stopEvent);
        _stopEvent = INVALID_HANDLE_VALUE;
    }
}

void FileWatcher::Impl::ThreadFunc()
{
    // Maximum watches we can multiplex (WFMO limit minus 1 for the stop event).
    static constexpr DWORD kMaxWatches = MAXIMUM_WAIT_OBJECTS - 1;

    while (_running) {
        // Build the wait list: stop event first, then each pending changeEvent.
        std::vector<HANDLE>      waitHandles;
        std::vector<WatchHandle> waitKeys;

        waitHandles.push_back(_stopEvent); // index 0 → stop

        {
            std::lock_guard lock{_watchesMtx};

            for (auto& [h, e] : _watches) {
                if (waitHandles.size() >= kMaxWatches + 1) break; // cap at limit

                // Issue RDCW for any watch that doesn't have one pending.
                if (!e.rdcwPending) {
                    IssueRdcw(e);
                }

                if (e.rdcwPending && e.changeEvent != INVALID_HANDLE_VALUE) {
                    waitHandles.push_back(e.changeEvent);
                    waitKeys.push_back(h);
                }
            }
        }

        DWORD count  = static_cast<DWORD>(waitHandles.size());
        DWORD result = WaitForMultipleObjects(count, waitHandles.data(),
                                              FALSE, 100 /*ms*/);

        if (!_running) return;
        if (result == WAIT_OBJECT_0) return;            // stop event
        if (result == WAIT_TIMEOUT)  continue;          // normal timeout
        if (result == WAIT_FAILED)   break;

        // A change event fired.
        DWORD idx = result - WAIT_OBJECT_0; // 1-based (0 = stop event)
        if (idx == 0 || idx >= count) continue;
        WatchHandle wh = waitKeys[idx - 1];

        std::lock_guard lock{_watchesMtx};
        auto it = _watches.find(wh);
        if (it == _watches.end()) continue;

        auto pushFn = [this](FileEvent ev) { PushEvent(std::move(ev)); };
        ProcessRdcwCompletion(it->second, pushFn);

        // Reissue RDCW immediately so the next change is captured.
        IssueRdcw(it->second);
    }
}

Result<WatchHandle> FileWatcher::Impl::AddWatch(const Path& path)
{
    HANDLE hDir = CreateFileW(
        path.Native().wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (hDir == INVALID_HANDLE_VALUE) return std::unexpected(Error::WatchFailed);

    // Auto-reset event — set by the OS when the overlapped RDCW completes.
    HANDLE hEvent = CreateEventW(nullptr, /*manualReset=*/FALSE, FALSE, nullptr);
    if (hEvent == INVALID_HANDLE_VALUE) {
        CloseHandle(hDir);
        return std::unexpected(Error::WatchFailed);
    }

    WatchHandle handle = _nextHandle.fetch_add(1);

    std::lock_guard lock{_watchesMtx};
    auto [it, ok] = _watches.emplace(handle, WatchEntry{});
    WatchEntry& e = it->second;
    e.path        = path;
    e.handle      = handle;
    e.dirHandle   = hDir;
    e.changeEvent = hEvent;

    // Issue the first overlapped RDCW NOW so any changes that happen
    // before the thread's next iteration are captured by the OS.
    IssueRdcw(e);

    return handle;
}

void FileWatcher::Impl::RemoveWatch(WatchHandle handle)
{
    std::lock_guard lock{_watchesMtx};
    auto it = _watches.find(handle);
    if (it == _watches.end()) return;
    if (it->second.dirHandle != INVALID_HANDLE_VALUE) {
        CancelIoEx(it->second.dirHandle, nullptr);
        CloseHandle(it->second.dirHandle);
        it->second.dirHandle = INVALID_HANDLE_VALUE;
    }
    if (it->second.changeEvent != INVALID_HANDLE_VALUE) {
        CloseHandle(it->second.changeEvent);
        it->second.changeEvent = INVALID_HANDLE_VALUE;
    }
    _watches.erase(it);
}

// =============================================================================
//  ███╗   ███╗ █████╗  ██████╗ ██████╗ ███████╗
//  ████╗ ████║██╔══██╗██╔════╝██╔═══██╗██╔════╝
//  ██╔████╔██║███████║██║     ██║   ██║███████╗
//  ██║╚██╔╝██║██╔══██║██║     ██║   ██║╚════██║
//  ██║ ╚═╝ ██║██║  ██║╚██████╗╚██████╔╝███████║
//  ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═════╝╚══════╝
// =============================================================================
#elif defined(__APPLE__)

FileWatcher::Impl::Impl()
{
    _kq = kqueue();
    if (pipe(_stopPipe) == 0) {
        fcntl(_stopPipe[0], F_SETFL, O_NONBLOCK);
        fcntl(_stopPipe[1], F_SETFL, O_NONBLOCK);
        // Register the stop pipe read-end with kqueue.
        struct kevent ke{};
        EV_SET(&ke, static_cast<uintptr_t>(_stopPipe[0]),
               EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (_kq >= 0) kevent(_kq, &ke, 1, nullptr, 0, nullptr);
    }
}

FileWatcher::Impl::~Impl() { Stop(); }

void FileWatcher::Impl::Start()
{
    _running = true;
    _thread  = std::jthread{[this](std::stop_token /*st*/) { ThreadFunc(); }};
}

void FileWatcher::Impl::Stop()
{
    if (!_running.exchange(false)) return;
    // Write one byte to wake the kqueue kevent() call. std::jthread joins
    // automatically when _thread is destructed after Stop() returns.
    if (_stopPipe[1] >= 0) { char c = 1; (void)write(_stopPipe[1], &c, 1); }

    std::lock_guard lock{_watchesMtx};
    for (auto& [h, e] : _watches) {
        if (e.fd >= 0) { close(e.fd); e.fd = -1; }
    }
    _watches.clear();

    if (_kq >= 0) { close(_kq); _kq = -1; }
    if (_stopPipe[0] >= 0) { close(_stopPipe[0]); _stopPipe[0] = -1; }
    if (_stopPipe[1] >= 0) { close(_stopPipe[1]); _stopPipe[1] = -1; }
}

void FileWatcher::Impl::ThreadFunc()
{
    while (_running) {
        // Register any un-registered watches.
        {
            std::lock_guard lock{_watchesMtx};
            for (auto& [handle, entry] : _watches) {
                if (!entry.registered && entry.fd >= 0) {
                    struct kevent ke{};
                    EV_SET(&ke,
                           static_cast<uintptr_t>(entry.fd),
                           EVFILT_VNODE,
                           EV_ADD | EV_CLEAR,
                           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME |
                           NOTE_ATTRIB | NOTE_CREATE,
                           0,
                           // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                           reinterpret_cast<void*>(static_cast<uintptr_t>(handle)));
                    kevent(_kq, &ke, 1, nullptr, 0, nullptr);
                    entry.registered = true;
                }
            }
        }

        struct kevent events[64];
        struct timespec timeout{0, 50L * 1000 * 1000}; // 50 ms
        int count = kevent(_kq, nullptr, 0, events, 64, &timeout);

        for (int i = 0; i < count; ++i) {
            const auto& ev = events[i];
            // Stop signal.
            if (_stopPipe[0] >= 0 &&
                ev.filter == EVFILT_READ &&
                static_cast<int>(ev.ident) == _stopPipe[0])
            {
                return;
            }
            if (ev.filter != EVFILT_VNODE) continue;

            auto whandle = static_cast<WatchHandle>(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<uintptr_t>(ev.udata));

            Path watchedPath;
            {
                std::lock_guard lock{_watchesMtx};
                auto it = _watches.find(whandle);
                if (it == _watches.end()) continue;
                watchedPath = it->second.path;
            }

            FileEvent fev;
            fev.ChangedPath = watchedPath;
            if      (ev.fflags & NOTE_DELETE) fev.Type = FileChangeType::Deleted;
            else if (ev.fflags & NOTE_RENAME) fev.Type = FileChangeType::Renamed;
            else if (ev.fflags & NOTE_CREATE) fev.Type = FileChangeType::Created;
            else                              fev.Type = FileChangeType::Modified;
            PushEvent(std::move(fev));
        }
    }
}

Result<WatchHandle> FileWatcher::Impl::AddWatch(const Path& path)
{
    int fd = open(path.ToString().c_str(), O_RDONLY);
    if (fd < 0) return std::unexpected(Error::WatchFailed);

    WatchHandle handle = _nextHandle.fetch_add(1);
    WatchEntry entry;
    entry.path       = path;
    entry.handle     = handle;
    entry.fd         = fd;
    entry.registered = false;

    std::lock_guard lock{_watchesMtx};
    _watches.emplace(handle, std::move(entry));
    return handle;
}

void FileWatcher::Impl::RemoveWatch(WatchHandle handle)
{
    std::lock_guard lock{_watchesMtx};
    auto it = _watches.find(handle);
    if (it == _watches.end()) return;
    if (it->second.fd >= 0) {
        // Remove from kqueue by closing the fd (kqueue auto-removes it).
        close(it->second.fd);
    }
    _watches.erase(it);
}

// =============================================================================
//  ██╗     ██╗███╗   ██╗██╗   ██╗██╗  ██╗
//  ██║     ██║████╗  ██║██║   ██║╚██╗██╔╝
//  ██║     ██║██╔██╗ ██║██║   ██║ ╚███╔╝
//  ██║     ██║██║╚██╗██║██║   ██║ ██╔██╗
//  ███████╗██║██║ ╚████║╚██████╔╝██╔╝ ██╗
//  ╚══════╝╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝
// =============================================================================
#else // Linux / POSIX

FileWatcher::Impl::Impl()
{
    _inotifyFd = inotify_init1(IN_NONBLOCK);
    if (pipe(_stopPipe) == 0) {
        fcntl(_stopPipe[0], F_SETFL, O_NONBLOCK);
        fcntl(_stopPipe[1], F_SETFL, O_NONBLOCK);
    }
}

FileWatcher::Impl::~Impl() { Stop(); }

void FileWatcher::Impl::Start()
{
    _running = true;
    _thread  = std::jthread{[this](std::stop_token /*st*/) { ThreadFunc(); }};
}

void FileWatcher::Impl::Stop()
{
    if (!_running.exchange(false)) return;
    // Write one byte to wake the select() call. std::jthread joins
    // automatically when _thread is destructed after Stop() returns.
    if (_stopPipe[1] >= 0) { char c = 1; (void)write(_stopPipe[1], &c, 1); }

    // Remove all inotify watches.
    if (_inotifyFd >= 0) {
        std::lock_guard lock{_watchesMtx};
        for (auto& [h, e] : _watches) {
            if (e.wd >= 0) inotify_rm_watch(_inotifyFd, e.wd);
        }
        _watches.clear();
        close(_inotifyFd);
        _inotifyFd = -1;
    }

    if (_stopPipe[0] >= 0) { close(_stopPipe[0]); _stopPipe[0] = -1; }
    if (_stopPipe[1] >= 0) { close(_stopPipe[1]); _stopPipe[1] = -1; }
}

void FileWatcher::Impl::ThreadFunc()
{
    // Build a reverse map: inotify wd → WatchHandle.
    auto BuildWdMap = [this]() {
        std::unordered_map<int, WatchHandle> m;
        std::lock_guard lock{_watchesMtx};
        for (auto& [handle, entry] : _watches) {
            if (entry.wd >= 0) m[entry.wd] = handle;
        }
        return m;
    };

    while (_running) {
        auto wdMap = BuildWdMap();

        fd_set rfds;
        FD_ZERO(&rfds);
        int maxFd = -1;
        if (_inotifyFd >= 0)  { FD_SET(_inotifyFd,  &rfds); maxFd = std::max(maxFd, _inotifyFd); }
        if (_stopPipe[0] >= 0){ FD_SET(_stopPipe[0], &rfds); maxFd = std::max(maxFd, _stopPipe[0]); }
        if (maxFd < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

        struct timeval tv{0, 50 * 1000};
        int ret = select(maxFd + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) break;
        if (_stopPipe[0] >= 0 && FD_ISSET(_stopPipe[0], &rfds)) break;
        if (_inotifyFd < 0 || !FD_ISSET(_inotifyFd, &rfds)) continue;

        alignas(struct inotify_event) char buf[4096];
        ssize_t len = read(_inotifyFd, buf, sizeof(buf));
        if (len <= 0) continue;

        for (const char* ptr = buf; ptr < buf + len;) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const auto* ev = reinterpret_cast<const struct inotify_event*>(ptr);

            auto it = wdMap.find(ev->wd);
            if (it != wdMap.end()) {
                Path watchedPath;
                {
                    std::lock_guard lock{_watchesMtx};
                    auto wit = _watches.find(it->second);
                    if (wit != _watches.end()) watchedPath = wit->second.path;
                }
                if (!watchedPath.IsEmpty()) {
                    FileEvent fev;
                    fev.ChangedPath = (ev->len > 0)
                        ? watchedPath / std::string_view{ev->name}
                        : watchedPath;

                    if      (ev->mask & (IN_CREATE | IN_MOVED_TO))   fev.Type = FileChangeType::Created;
                    else if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) fev.Type = FileChangeType::Deleted;
                    else                                              fev.Type = FileChangeType::Modified;
                    PushEvent(std::move(fev));
                }
            }
            ptr += sizeof(struct inotify_event) + ev->len;
        }
    }
}

Result<WatchHandle> FileWatcher::Impl::AddWatch(const Path& path)
{
    if (_inotifyFd < 0) return std::unexpected(Error::WatchFailed);

    int wd = inotify_add_watch(_inotifyFd, path.ToString().c_str(),
        IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd < 0) return std::unexpected(Error::WatchFailed);

    WatchHandle handle = _nextHandle.fetch_add(1);
    WatchEntry entry;
    entry.path   = path;
    entry.handle = handle;
    entry.wd     = wd;

    std::lock_guard lock{_watchesMtx};
    _watches.emplace(handle, std::move(entry));
    return handle;
}

void FileWatcher::Impl::RemoveWatch(WatchHandle handle)
{
    std::lock_guard lock{_watchesMtx};
    auto it = _watches.find(handle);
    if (it == _watches.end()) return;
    if (_inotifyFd >= 0 && it->second.wd >= 0)
        inotify_rm_watch(_inotifyFd, it->second.wd);
    _watches.erase(it);
}

#endif // platform

// =============================================================================
// FileWatcher public interface (shared for all platforms)
// =============================================================================

FileWatcher::FileWatcher()
    : _impl(std::make_unique<Impl>())
{
    _impl->Start();
}

FileWatcher::~FileWatcher() noexcept = default;

FileWatcher::FileWatcher(FileWatcher&&) noexcept = default;
FileWatcher& FileWatcher::operator=(FileWatcher&&) noexcept = default;

Result<WatchHandle> FileWatcher::Watch(const Path& path)
{
#if defined(__EMSCRIPTEN__)
    // No background file monitoring thread is created on Emscripten — see
    // the Phase 23 proposal's Utility Layer on Emscripten section.
    (void)path;
    return std::unexpected(Error::NotSupported);
#else
    return _impl->AddWatch(path);
#endif
}

void FileWatcher::Unwatch(WatchHandle handle)
{
    if (handle == InvalidWatchHandle) return;
    _impl->RemoveWatch(handle);
}

uint32_t FileWatcher::Poll(const std::function<void(const FileEvent&)>& handler)
{
    std::deque<FileEvent> local;
    {
        std::lock_guard lock{_impl->_queueMtx};
        std::swap(local, _impl->_queue);
    }
    for (const auto& ev : local) {
        handler(ev);
    }
    return static_cast<uint32_t>(local.size());
}

} // namespace ImFrame::Utility
