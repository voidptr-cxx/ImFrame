/**
 * @file     CommandBuffer.hpp
 * @brief    Renderer-agnostic recorded draw commands emitted by the widget tree
 *
 * `CommandBuffer` is the abstract drawing language `Tree::Element::Paint()`
 * speaks after Phase 31. Rather than calling ImGui draw-list functions
 * directly, a `Paint()` override that only draws (no widget/interaction
 * state — see the file comment on `Internal::ImGuiCompatRenderer` for the
 * split) records `Command` values into the buffer passed down through the
 * tree. A renderer (`Internal::IRenderer`; today, `Internal::
 * ImGuiCompatRenderer`) reads the finished buffer once per frame and turns
 * each command into real draw calls.
 *
 * @internal
 * Recording never executes anything — `Push()` only appends to a
 * `std::vector`. `CommandBuffer::Reset()` clears that vector (`clear()`
 * keeps its allocated capacity) rather than destroying and reallocating it,
 * so a `CommandBuffer` instance reused frame-to-frame settles into zero
 * allocations once its capacity has grown to cover a frame's command count —
 * this is the "pool" the Phase 31 proposal describes; there is exactly one
 * buffer in flight at a time (the render loop is single-threaded and
 * synchronous, so a literal multi-buffer pool would have nothing to
 * pipeline against). See `.claude/DECISIONS.md`, Phase 31.1.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-16
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ImFrame::Rendering {

// ─── Opaque handles ─────────────────────────────────────────────────────────

/**
 * @class    TextureId
 * @brief    Opaque, renderer-interpreted texture handle
 *
 * Each renderer gives this value its own meaning: `ImGuiCompatRenderer`
 * treats it as an `ImTextureID`; a future native renderer (Phase 32) treats
 * it as an index into its own texture registry. Never construct one
 * directly — obtain it from `ImageLoader::Load()` or `Viewport::TextureId()`.
 *
 * @since    3.0.0
 */
class TextureId {
public:
    constexpr TextureId() noexcept = default;

    /// @internal Constructed by whichever subsystem issues texture handles.
    constexpr explicit TextureId(std::uint64_t value) noexcept : _value(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept { return _value; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return _value != 0; }

    [[nodiscard]] friend constexpr bool operator==(TextureId, TextureId) noexcept = default;

private:
    std::uint64_t _value = 0;
};

/**
 * @class    FontId
 * @brief    Opaque, renderer-interpreted font handle
 *
 * The text rendering system (Phase 33) maps `FontId` to font metrics; the
 * `ImGuiCompatRenderer` maps it to an `ImFont*`. A default-constructed
 * `FontId` means "renderer's default font."
 *
 * @since    3.0.0
 */
class FontId {
public:
    constexpr FontId() noexcept = default;

    /// @internal Constructed by whichever subsystem issues font handles.
    constexpr explicit FontId(std::uint32_t value) noexcept : _value(value) {}

    [[nodiscard]] constexpr std::uint32_t Value() const noexcept { return _value; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return _value != 0; }

    [[nodiscard]] friend constexpr bool operator==(FontId, FontId) noexcept = default;

private:
    std::uint32_t _value = 0;
};

// ─── Corner radii ───────────────────────────────────────────────────────────

/// Per-corner rounding, shared by `DrawRect`/`DrawImage`/`DrawShadow`.
struct CornerRadii {
    float TopLeft     = 0.0f;
    float TopRight    = 0.0f;
    float BottomRight = 0.0f;
    float BottomLeft  = 0.0f;

    /// Equal rounding on all four corners.
    [[nodiscard]] static constexpr CornerRadii All(float r) noexcept { return {r, r, r, r}; }
};

// ─── Command payloads ───────────────────────────────────────────────────────

/// Filled and/or stroked rectangle with independent per-corner rounding.
struct DrawRect {
    Widgets::Vec2 Position{};
    Widgets::Vec2 Size{};
    CornerRadii   Radii{};
    Widgets::Vec4 FillColor{0.0f, 0.0f, 0.0f, 0.0f};
    Widgets::Vec4 StrokeColor{0.0f, 0.0f, 0.0f, 0.0f};
    float         StrokeWidth = 0.0f;
};

/// A run of text at a position, optionally wrapped at `MaxWidth`.
struct DrawText {
    Widgets::Vec2 Position{};
    std::string   Text;
    FontId        Font{};
    float         FontSize = 0.0f; ///< `<= 0` uses the renderer's default size.
    Widgets::Vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
    float         MaxWidth = 0.0f; ///< `<= 0` disables wrapping.
};

/// A textured rectangle, with independent per-corner rounding.
struct DrawImage {
    Widgets::Vec2 Position{};
    Widgets::Vec2 Size{};
    TextureId     Texture{};
    Widgets::Vec2 UvMin{0.0f, 0.0f};
    Widgets::Vec2 UvMax{1.0f, 1.0f};
    Widgets::Vec4 TintColor{1.0f, 1.0f, 1.0f, 1.0f};
    CornerRadii   Radii{};
};

/// One segment of a `DrawPath`'s point sequence.
enum class PathCommandType : unsigned char {
    MoveTo,  ///< Start a new subpath at `Point`.
    LineTo,  ///< Straight segment to `Point`.
    CurveTo, ///< Cubic Bézier segment to `Point`, via `Control1`/`Control2`.
    Close,   ///< Close the current subpath back to its start.
};

/// One point/curve segment within a `DrawPath`.
struct PathCommand {
    PathCommandType Type = PathCommandType::MoveTo;
    Widgets::Vec2   Point{};
    Widgets::Vec2   Control1{}; ///< First control point; only meaningful for `CurveTo`.
    Widgets::Vec2   Control2{}; ///< Second control point; only meaningful for `CurveTo`.
};

/// Fill rule for a filled `DrawPath`.
enum class FillRule : unsigned char {
    NonZero, ///< Non-zero winding rule (the common case).
    EvenOdd, ///< Even-odd winding rule.
};

/// An arbitrary filled/stroked path built from `PathCommand` segments.
struct DrawPath {
    std::vector<PathCommand> Commands;
    Widgets::Vec4            FillColor{0.0f, 0.0f, 0.0f, 0.0f};
    Widgets::Vec4            StrokeColor{0.0f, 0.0f, 0.0f, 0.0f};
    float                     StrokeWidth = 0.0f;
    FillRule                  Rule        = FillRule::NonZero;
};

/// A box shadow. The renderer decides how to implement it (blur pass, SDF, approximation, ...).
struct DrawShadow {
    Widgets::Vec2 Position{};
    Widgets::Vec2 Size{};
    CornerRadii   Radii{};
    float         BlurRadius = 0.0f;
    Widgets::Vec2 Offset{};
    Widgets::Vec4 ShadowColor{0.0f, 0.0f, 0.0f, 0.5f};
    float         Spread = 0.0f;
};

/// Pushes an axis-aligned clip rectangle; subsequent commands are clipped until the matching `PopClipRect`.
struct PushClipRect {
    Widgets::Vec2 Position{};
    Widgets::Vec2 Size{};
    float         CornerRadius = 0.0f; ///< Non-zero for a rounded clip region.
};

/// Pops one clip rectangle pushed by `PushClipRect`.
struct PopClipRect {};

/// Begins a compositing layer at the given opacity; subsequent commands render into it until `PopLayer`.
struct PushOpacityLayer {
    float Opacity = 1.0f;
};

/// Blend mode for a `PushBlendLayer`.
enum class BlendMode : unsigned char {
    Normal,   ///< Standard alpha-over compositing.
    Multiply, ///< Multiply blend.
    Screen,   ///< Screen blend.
    Additive, ///< Additive blend.
};

/// Begins a compositing layer with a specified blend mode; subsequent commands render into it until `PopLayer`.
struct PushBlendLayer {
    BlendMode Mode = BlendMode::Normal;
};

/// Composites the top layer (opened by `PushOpacityLayer` or `PushBlendLayer`) into the layer below.
struct PopLayer {};

/**
 * @typedef  Command
 * @brief    One recorded drawing operation
 *
 * @since    3.0.0
 */
using Command = std::variant<DrawRect, DrawText, DrawImage, DrawPath, DrawShadow, PushClipRect, PopClipRect,
                              PushOpacityLayer, PushBlendLayer, PopLayer>;

// ─── CommandBuffer ──────────────────────────────────────────────────────────

/**
 * @class    CommandBuffer
 * @brief    An ordered, replayable sequence of `Command` values recorded during one `Paint()` pass
 *
 * @since    3.0.0
 *
 * @example
 * @code
 * void BoxElement::Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) {
 *     cmd.Push(Rendering::DrawRect{
 *         .Position = position, .Size = _size,
 *         .FillColor = _config.GetBackground(),
 *     });
 *     if (_child) { (*_child)->Paint(cmd, position); }
 * }
 * @endcode
 */
class CommandBuffer {
public:
    /// Appends one recorded command.
    void Push(Command command) { _commands.push_back(std::move(command)); }

    /// Clears recorded commands for a new frame, retaining allocated capacity (no reallocation once warmed up).
    void Reset() noexcept { _commands.clear(); }

    [[nodiscard]] std::size_t Size() const noexcept { return _commands.size(); }
    [[nodiscard]] bool        Empty() const noexcept { return _commands.empty(); }

    /// Currently allocated storage, in commands. Exposed so tests/diagnostics can confirm `Reset()` retains capacity.
    [[nodiscard]] std::size_t Capacity() const noexcept { return _commands.capacity(); }

    [[nodiscard]] const std::vector<Command>& Commands() const noexcept { return _commands; }

    [[nodiscard]] auto begin() const noexcept { return _commands.begin(); }
    [[nodiscard]] auto end() const noexcept { return _commands.end(); }

private:
    std::vector<Command> _commands;
};

} // namespace ImFrame::Rendering
