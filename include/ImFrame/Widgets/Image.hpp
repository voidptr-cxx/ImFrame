/**
 * @file     Image.hpp
 * @brief    GPU-texture image display and image-button widget
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
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    Image
 * @brief    Displays a GPU texture via `ImGui::Image()`; optionally as a clickable button
 *
 * UV coordinates default to the full texture `(0,0)→(1,1)`. Tint defaults to
 * white (no tint). Border colour defaults to transparent (no border).
 *
 * `Show()` always returns `false` — images are not interactive.
 * `ShowButton()` wraps with `ImGui::ImageButton()` and returns `true` on click.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Image(myTex, {256.0f, 256.0f})
 *     .Tint({1.0f, 1.0f, 1.0f, 0.8f})
 *     .Show();
 *
 * if (Widgets::Image(iconTex, {32.0f, 32.0f}).ShowButton()) { DoAction(); }
 * @endcode
 */
/// @deprecated Use `ImageWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.2.
class [[deprecated("Use ImageWidget instead. See Docs/Migration_v1_to_v2.md.")]] Image {
public:
    /**
     * @brief    Construct an image widget.
     * @param[in]  texture  GPU texture handle (ImTextureID).
     * @param[in]  size     Display size in pixels.
     * @throws   Nothing.
     */
    explicit Image(TextureHandle texture, Vec2 size)
        : _texture(texture), _size(size) {}

    Image& UV0(Vec2 uv)          { _uv0 = uv;          return *this; }
    Image& UV1(Vec2 uv)          { _uv1 = uv;          return *this; }
    Image& Tint(Vec4 tint)       { _tint = tint;        return *this; }
    Image& BorderColor(Vec4 col) { _border = col;       return *this; }
    Image& Disabled(bool d = true) { _disabled = d;    return *this; }
    Image& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    Image& Width(float w)        { _width = w;          return *this; }
    Image& Id(std::string_view id) { _id = id;          return *this; }

    /**
     * @brief    Render the image. Always returns `false`.
     * @return   `false` — images are not interactive.
     * @throws   Nothing.
     */
    bool Show();

    /**
     * @brief    Render the image as a clickable button.
     * @return   `true` if clicked this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool ShowButton();

private:
    TextureHandle       _texture;
    Vec2                _size;
    Vec2                _uv0      {0.0f, 0.0f};
    Vec2                _uv1      {1.0f, 1.0f};
    Vec4                _tint     {1.0f, 1.0f, 1.0f, 1.0f};
    Vec4                _border   {0.0f, 0.0f, 0.0f, 0.0f};
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

// ─── ImageWidget (Phase 30) ─────────────────────────────────────────────────────

/**
 * @class    ImageWidget
 * @brief    Declarative GPU-texture image — `Tree::PrimitiveWidget` replacement for `Image`
 *
 * Collapses the old dual `Show()`/`ShowButton()` API into a single widget:
 * calls `ImGui::Image()` when no `OnClick` is set, or `ImGui::ImageButton()`
 * when one is — mirroring how `ButtonWidget`/`CheckboxWidget` fold their
 * interaction into one `OnClick`/`OnChange` delegate rather than a dual
 * call-path. See `DECISIONS.md` (Phase 30.1).
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * ImageWidget(myTex, {256.0f, 256.0f}).Tint({1.0f, 1.0f, 1.0f, 0.8f});
 * ImageWidget(iconTex, {32.0f, 32.0f}).OnClick([&] { DoAction(); });
 * @endcode
 */
class ImageWidget {
public:
    ImageWidget(TextureHandle texture, Vec2 size) : _texture(texture), _size(size) {}

    ImageWidget& UV0(Vec2 uv) noexcept { _uv0 = uv; return *this; }
    ImageWidget& UV1(Vec2 uv) noexcept { _uv1 = uv; return *this; }
    ImageWidget& Tint(Vec4 tint) noexcept { _tint = tint; return *this; }
    ImageWidget& BorderColor(Vec4 col) noexcept { _border = col; return *this; }
    ImageWidget& OnClick(Utility::Delegate<void()> cb) { _onClick = std::move(cb); return *this; }
    ImageWidget& Disabled(bool d = true) noexcept { _disabled = d; return *this; }
    ImageWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    ImageWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] TextureHandle GetTexture() const noexcept { return _texture; }
    [[nodiscard]] Vec2 GetSize() const noexcept { return _size; }
    [[nodiscard]] Vec2 GetUV0() const noexcept { return _uv0; }
    [[nodiscard]] Vec2 GetUV1() const noexcept { return _uv1; }
    [[nodiscard]] Vec4 GetTint() const noexcept { return _tint; }
    [[nodiscard]] Vec4 GetBorderColor() const noexcept { return _border; }
    [[nodiscard]] const Utility::Delegate<void()>& GetOnClick() const noexcept { return _onClick; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }

    /// @internal Produces this image's concrete `Element`. Defined in `Image.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    TextureHandle              _texture;
    Vec2                       _size;
    Vec2                       _uv0    {0.0f, 0.0f};
    Vec2                       _uv1    {1.0f, 1.0f};
    Vec4                       _tint   {1.0f, 1.0f, 1.0f, 1.0f};
    Vec4                       _border {0.0f, 0.0f, 0.0f, 0.0f};
    Utility::Delegate<void()>  _onClick;
    bool                       _disabled = false;
    std::string                _tooltip;
    Tree::Key                  _key;
};

} // namespace ImFrame::Widgets
