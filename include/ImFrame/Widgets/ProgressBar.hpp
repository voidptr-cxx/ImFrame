/**
 * @file     ProgressBar.hpp
 * @brief    Determinate progress bar widget
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    ProgressBar
 * @brief    Horizontal progress bar displaying a `[0, 1]` fraction
 *
 * `Show()` always returns `false` — progress bars are not interactive.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::ProgressBar(loadProgress)
 *     .Size({-1.0f, 0.0f})
 *     .Overlay("Loading assets...")
 *     .Show();
 * @endcode
 */
/// @deprecated Use `ProgressBarWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.2.
class [[deprecated("Use ProgressBarWidget instead. See Docs/Migration_v1_to_v2.md.")]] ProgressBar {
public:
    /**
     * @brief    Construct a progress bar.
     * @param[in]  fraction  Completion ratio in `[0, 1]`. Values outside this range are clamped by ImGui.
     * @throws   Nothing.
     */
    explicit ProgressBar(float fraction) : _fraction(fraction) {}

    /**
     * @brief    Set explicit display size.
     * @param[in]  size  Width × height in pixels. `-1` in either axis fills the available space.
     * @return   `*this` for chaining.
     */
    ProgressBar& Size(Vec2 size)            { _size = size;      return *this; }

    /**
     * @brief    Draw overlay text centred on the bar.
     * @param[in]  text  Text shown on top of the progress bar.
     * @return   `*this` for chaining.
     */
    ProgressBar& Overlay(std::string_view text) { _overlay = text; return *this; }

    ProgressBar& Disabled(bool d = true)    { _disabled = d;     return *this; }
    ProgressBar& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    ProgressBar& Width(float w)             { _width = w;        return *this; }
    ProgressBar& Id(std::string_view id)    { _id = id;          return *this; }

    /**
     * @brief    Render the progress bar. Always returns `false`.
     * @return   `false` — progress bars are not interactive.
     * @throws   Nothing.
     */
    bool Show();

private:
    float               _fraction;
    Vec2                _size     {-1.0f, 0.0f};
    std::string_view    _overlay;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

// ─── ProgressBarWidget (Phase 30) ───────────────────────────────────────────────

/**
 * @class    ProgressBarWidget
 * @brief    Declarative progress bar — `Tree::PrimitiveWidget` replacement for `ProgressBar`
 *
 * Produces the exact same `ImGui::ProgressBar()` call as `ProgressBar::Show()`.
 * Stateless — no caller-owned binding needed, `Fraction` is read fresh every
 * `Build()`.
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * ProgressBarWidget(loadProgress).Overlay("Loading assets...");
 * @endcode
 */
class ProgressBarWidget {
public:
    explicit ProgressBarWidget(float fraction) noexcept : _fraction(fraction) {}

    ProgressBarWidget& Size(Vec2 size) noexcept { _size = size; return *this; }
    ProgressBarWidget& Overlay(std::string text) { _overlay = std::move(text); return *this; }
    ProgressBarWidget& Disabled(bool d = true) noexcept { _disabled = d; return *this; }
    ProgressBarWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    ProgressBarWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    ProgressBarWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] float GetFraction() const noexcept { return _fraction; }
    [[nodiscard]] Vec2 GetSize() const noexcept { return _size; }
    [[nodiscard]] const std::string& GetOverlay() const noexcept { return _overlay; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    /// @internal Produces this progress bar's concrete `Element`. Defined in `ProgressBar.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    float       _fraction;
    Vec2        _size     {-1.0f, 0.0f};
    std::string _overlay;
    bool        _disabled = false;
    std::string _tooltip;
    float       _width    = 0.0f;
    Tree::Key   _key;
};

} // namespace ImFrame::Widgets
