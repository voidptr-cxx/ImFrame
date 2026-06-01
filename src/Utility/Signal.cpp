/**
 * @file     Signal.cpp
 * @brief    Connection RAII handle implementation
 *
 * @internal
 * Contains the non-template Connection class implementation. Signal<Sig> and
 * ThreadSafeSignal<Sig> are fully defined in Signal.hpp as they are templates.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/Signal.hpp"

namespace ImFrame::Utility {

Connection::Connection(std::function<void()> disconnectFn)
    : _disconnectFn{std::move(disconnectFn)} {}

Connection::~Connection() {
    Disconnect();
}

Connection::Connection(Connection&& other) noexcept
    : _disconnectFn{std::move(other._disconnectFn)} {
    other._disconnectFn = nullptr;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        Disconnect();
        _disconnectFn       = std::move(other._disconnectFn);
        other._disconnectFn = nullptr;
    }
    return *this;
}

void Connection::Disconnect() noexcept {
    if (_disconnectFn) {
        _disconnectFn();
        _disconnectFn = nullptr;
    }
}

bool Connection::IsConnected() const noexcept {
    return static_cast<bool>(_disconnectFn);
}

} // namespace ImFrame::Utility
