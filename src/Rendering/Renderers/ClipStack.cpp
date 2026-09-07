/**
 * @file     ClipStack.cpp
 * @brief    `ClipStack` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ClipStack.hpp"

#include "ImFrame/Core/Error.hpp"

#include <algorithm>

namespace ImFrame::Internal {

void ClipStack::Push(const Rendering::PushClipRect& cmd) {
    ClipRect incoming{.Position = cmd.Position, .Size = cmd.Size, .CornerRadius = cmd.CornerRadius};

    if (_entries.empty()) {
        _entries.push_back(Entry{.Effective = incoming, .HasRounded = cmd.CornerRadius > 0.0f});
        return;
    }

    const ClipRect& parent = _entries.back().Effective;

    const float minX = std::max(parent.Position.x, incoming.Position.x);
    const float minY = std::max(parent.Position.y, incoming.Position.y);
    const float maxX = std::min(parent.Position.x + parent.Size.x, incoming.Position.x + incoming.Size.x);
    const float maxY = std::min(parent.Position.y + parent.Size.y, incoming.Position.y + incoming.Size.y);

    ClipRect effective{.Position = {minX, minY},
                        .Size = {std::max(0.0f, maxX - minX), std::max(0.0f, maxY - minY)},
                        .CornerRadius = cmd.CornerRadius};

    _entries.push_back(
        Entry{.Effective = effective, .HasRounded = _entries.back().HasRounded || cmd.CornerRadius > 0.0f});
}

void ClipStack::Pop() {
    IMF_ASSERT(!_entries.empty());
    if (!_entries.empty()) { _entries.pop_back(); }
}

void ClipStack::Reset() { _entries.clear(); }

const ClipRect& ClipStack::Current() const noexcept {
    IMF_ASSERT(!_entries.empty());
    static const ClipRect kEmpty{};
    return _entries.empty() ? kEmpty : _entries.back().Effective;
}

ClipKind ClipStack::CurrentKind() const noexcept {
    if (_entries.empty()) { return ClipKind::AxisAligned; }
    return _entries.back().HasRounded ? ClipKind::Rounded : ClipKind::AxisAligned;
}

} // namespace ImFrame::Internal
