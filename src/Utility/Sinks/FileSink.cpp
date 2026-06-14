/**
 * @file     FileSink.cpp
 * @brief    File-append log sink with byte-threshold rotation
 *
 * @internal
 * FileSink opens the file for appending on construction. On each Write(),
 * it checks the current file size. When rotateAtBytes > 0 and the size
 * reaches the threshold, the file is closed, renamed to <stem>.1.log (any
 * pre-existing .1.log is overwritten), then a new file is opened.
 *
 * Size tracking uses a running byte counter incremented on each Write(),
 * avoiding an fseek()/ftell() call per entry for better throughput.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/Logger.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace ImFrame::Utility {

// ─── FileSink::Impl ───────────────────────────────────────────────────────────

struct FileSink::Impl {
    std::filesystem::path  filePath;
    std::size_t            rotateAtBytes{0};
    std::size_t            bytesWritten{0};
    std::FILE*             file{nullptr};

    void Open() {
#ifdef _WIN32
        fopen_s(&file, filePath.string().c_str(), "ab");
#else
        file = std::fopen(filePath.string().c_str(), "ab"); // NOLINT(cert-err33-c)
#endif
        if (!file) throw std::runtime_error{"FileSink: cannot open " + filePath.string()};
        // Seed bytesWritten with current file size
        std::fseek(file, 0, SEEK_END);
        bytesWritten = static_cast<std::size_t>(std::ftell(file));
    }

    void Rotate() {
        if (file) { std::fclose(file); file = nullptr; }

        // Build: <parent>/<stem>.1<extension>  e.g. "app.log" → "app.1.log"
        std::filesystem::path rotated =
            filePath.parent_path() /
            (filePath.stem().string() + ".1" + filePath.extension().string());

        std::error_code ec;
        std::filesystem::rename(filePath, rotated, ec); // overwrite any existing .1.log
        (void)ec;

        Open();
    }
};

// ─── FileSink ─────────────────────────────────────────────────────────────────

FileSink::FileSink(const Path& path, std::size_t rotateAtBytes)
    : _impl{std::make_unique<Impl>()} {
    _impl->filePath      = path.Native();
    _impl->rotateAtBytes = rotateAtBytes;
    _impl->Open();
}

FileSink::~FileSink() noexcept {
    if (_impl && _impl->file) {
        std::fflush(_impl->file);
        std::fclose(_impl->file);
    }
}

void FileSink::Write(const LogEntry& entry) {
    if (!_impl->file) return;

    // Rotate before writing if threshold exceeded
    if (_impl->rotateAtBytes > 0 && _impl->bytesWritten >= _impl->rotateAtBytes) {
        _impl->Rotate();
    }

    // Format timestamp
    auto tt = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

    const char* levelNames[] = {"TRACE","DEBUG","INFO ","WARN ","ERROR","FATAL"};
    const char* levelName = levelNames[static_cast<int>(entry.level)];

    int written = std::fprintf(_impl->file, "[%s] %s %s:%d %s\n",
                               timeBuf,
                               levelName,
                               entry.location.file_name(),
                               static_cast<int>(entry.location.line()),
                               entry.message.c_str());
    if (written > 0) _impl->bytesWritten += static_cast<std::size_t>(written);
}

void FileSink::Flush() {
    if (_impl->file) std::fflush(_impl->file);
}

} // namespace ImFrame::Utility
