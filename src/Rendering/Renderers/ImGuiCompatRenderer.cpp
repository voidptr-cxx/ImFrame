/**
 * @file     ImGuiCompatRenderer.cpp
 * @brief    Implementation of the `CommandBuffer` → ImGui draw-list translation
 *
 * @internal
 * Translation table: `DrawRect` -> `AddRectFilled` + `AddRect`. `DrawText` ->
 * `AddText`. `DrawImage` -> `AddImage`/`AddImageRounded`. `DrawPath` -> the
 * stateful `Path*` API (`PathLineTo`/`PathBezierCubicCurveTo`/`PathFillConvex`/
 * `PathStroke`) — a convex-polygon approximation per the Phase 31 proposal;
 * `FillRule` is accepted but not applied (only correct for convex shapes
 * today). `DrawShadow` -> a handful of concentric, increasingly transparent
 * `AddRectFilled` rings (the proposal's own suggested approximation — replaced
 * by a proper shadow renderer in a later phase). `PushClipRect`/`PopClipRect`
 * -> the matching `ImDrawList` calls (rectangular only — `CornerRadius` is
 * accepted but rounded clip regions aren't supported at the draw-list level).
 * `PushOpacityLayer`/`PushBlendLayer`/`PopLayer` are currently structural
 * no-ops: true per-layer alpha/blend compositing needs an offscreen render
 * target ImGui's immediate-mode draw list doesn't provide, and nothing
 * produces these commands yet — see `.claude/DECISIONS.md`, Phase 31.2.
 *
 * `Rendering::FontId` is resolved via `ResolveFont()` (Phase 33.8) against
 * `LoadFont()`'s own `_fontsById` map — an invalid or unknown `FontId` still
 * falls back to `ImGui::GetFont()`, the active default font (pre-Phase-33.8
 * behaviour, preserved for every `DrawText` producer that never sets `Font`).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-16
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImGuiCompatRenderer.hpp"

#include "ImFrame/Utility/File.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>

namespace ImFrame::Internal {

namespace {

[[nodiscard]] ImU32 ToImU32(Widgets::Vec4 c) noexcept {
    auto channel = [](float v) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return IM_COL32(channel(c.x), channel(c.y), channel(c.z), channel(c.w));
}

/// Approximates independent per-corner radii with ImGui's single-rounding-value + per-corner-flags model.
struct ResolvedRounding {
    float       Radius = 0.0f;
    ImDrawFlags Flags  = ImDrawFlags_RoundCornersNone;
};

[[nodiscard]] ResolvedRounding ResolveRounding(Rendering::CornerRadii r) noexcept {
    const float maxRadius = std::max({r.TopLeft, r.TopRight, r.BottomRight, r.BottomLeft});
    if (maxRadius <= 0.0f) { return {}; }

    ImDrawFlags flags = ImDrawFlags_None;
    if (r.TopLeft > 0.0f) { flags |= ImDrawFlags_RoundCornersTopLeft; }
    if (r.TopRight > 0.0f) { flags |= ImDrawFlags_RoundCornersTopRight; }
    if (r.BottomRight > 0.0f) { flags |= ImDrawFlags_RoundCornersBottomRight; }
    if (r.BottomLeft > 0.0f) { flags |= ImDrawFlags_RoundCornersBottomLeft; }
    return {maxRadius, flags};
}

void Translate(ImDrawList* dl, const Rendering::DrawRect& rect) {
    const ImVec2 pMin{rect.Position.x, rect.Position.y};
    const ImVec2 pMax{rect.Position.x + rect.Size.x, rect.Position.y + rect.Size.y};
    const ResolvedRounding rounding = ResolveRounding(rect.Radii);

    if (rect.FillColor.w > 0.0f) {
        dl->AddRectFilled(pMin, pMax, ToImU32(rect.FillColor), rounding.Radius, rounding.Flags);
    }
    if (rect.StrokeColor.w > 0.0f && rect.StrokeWidth > 0.0f) {
        dl->AddRect(pMin, pMax, ToImU32(rect.StrokeColor), rounding.Radius, rect.StrokeWidth, rounding.Flags);
    }
}

void Translate(ImDrawList* dl, const Rendering::DrawText& text, ImFont* font) {
    const float fontSize = text.FontSize > 0.0f ? text.FontSize : ImGui::GetFontSize();
    const float wrapWidth = text.MaxWidth > 0.0f ? text.MaxWidth : 0.0f;
    const ImVec2 pos{text.Position.x, text.Position.y};
    dl->AddText(font, fontSize, pos, ToImU32(text.Color), text.Text.c_str(),
                text.Text.c_str() + text.Text.size(), wrapWidth);
}

void Translate(ImDrawList* dl, const Rendering::DrawImage& image) {
    const ImTextureRef texRef{static_cast<ImTextureID>(image.Texture.Value())};
    const ImVec2 pMin{image.Position.x, image.Position.y};
    const ImVec2 pMax{image.Position.x + image.Size.x, image.Position.y + image.Size.y};
    const ImVec2 uv0{image.UvMin.x, image.UvMin.y};
    const ImVec2 uv1{image.UvMax.x, image.UvMax.y};
    const ResolvedRounding rounding = ResolveRounding(image.Radii);
    const ImU32 tint = ToImU32(image.TintColor);

    if (rounding.Radius > 0.0f) {
        dl->AddImageRounded(texRef, pMin, pMax, uv0, uv1, tint, rounding.Radius, rounding.Flags);
    } else {
        dl->AddImage(texRef, pMin, pMax, uv0, uv1, tint);
    }
}

void Translate(ImDrawList* dl, const Rendering::DrawPath& path) {
    const bool hasClose = std::any_of(path.Commands.begin(), path.Commands.end(),
        [](const Rendering::PathCommand& c) { return c.Type == Rendering::PathCommandType::Close; });

    auto buildPath = [&] {
        dl->PathClear();
        for (const Rendering::PathCommand& cmd : path.Commands) {
            switch (cmd.Type) {
                case Rendering::PathCommandType::MoveTo:
                case Rendering::PathCommandType::LineTo:
                    dl->PathLineTo(ImVec2{cmd.Point.x, cmd.Point.y});
                    break;
                case Rendering::PathCommandType::CurveTo:
                    dl->PathBezierCubicCurveTo(ImVec2{cmd.Control1.x, cmd.Control1.y},
                                                ImVec2{cmd.Control2.x, cmd.Control2.y},
                                                ImVec2{cmd.Point.x, cmd.Point.y});
                    break;
                case Rendering::PathCommandType::Close:
                    break; // handled via ImDrawFlags_Closed on stroke, below
            }
        }
    };

    // FillRule is accepted but not applied — AddConvexPolyFilled only produces
    // correct results for convex shapes, matching the proposal's own framing
    // of DrawPath as a convex-polygon approximation for this compat renderer.
    if (path.FillColor.w > 0.0f) {
        buildPath();
        dl->PathFillConvex(ToImU32(path.FillColor)); // clears the path afterward
    }
    if (path.StrokeColor.w > 0.0f && path.StrokeWidth > 0.0f) {
        buildPath();
        dl->PathStroke(ToImU32(path.StrokeColor), path.StrokeWidth, hasClose ? ImDrawFlags_Closed : ImDrawFlags_None);
    }
}

void Translate(ImDrawList* dl, const Rendering::DrawShadow& shadow) {
    constexpr int kRings = 6;
    const Widgets::Vec2 basePos{shadow.Position.x + shadow.Offset.x, shadow.Position.y + shadow.Offset.y};

    for (int i = 0; i < kRings; ++i) {
        const float t      = static_cast<float>(i) / static_cast<float>(kRings - 1); // 0..1
        const float expand = shadow.Spread + t * shadow.BlurRadius;

        const ImVec2 pMin{basePos.x - expand, basePos.y - expand};
        const ImVec2 pMax{basePos.x + shadow.Size.x + expand, basePos.y + shadow.Size.y + expand};

        Widgets::Vec4 ringColor = shadow.ShadowColor;
        ringColor.w *= (1.0f - t) * (1.0f - t); // fade faster for outer rings

        const float ringRounding = std::max({shadow.Radii.TopLeft, shadow.Radii.TopRight,
                                              shadow.Radii.BottomRight, shadow.Radii.BottomLeft}) + expand;
        dl->AddRectFilled(pMin, pMax, ToImU32(ringColor), ringRounding);
    }
}

void Translate(ImDrawList* dl, const Rendering::PushClipRect& clip) {
    // CornerRadius is accepted but not applied — rounded clip regions aren't
    // supported at the ImDrawList level; rectangular clipping only, today.
    const ImVec2 pMin{clip.Position.x, clip.Position.y};
    const ImVec2 pMax{clip.Position.x + clip.Size.x, clip.Position.y + clip.Size.y};
    dl->PushClipRect(pMin, pMax, true);
}

} // namespace

ImFont* ImGuiCompatRenderer::ResolveFont(Rendering::FontId id) const {
    if (!id.IsValid()) { return ImGui::GetFont(); }
    const auto it = _fontsById.find(id.Value());
    return it != _fontsById.end() ? it->second : ImGui::GetFont();
}

Result<Rendering::FontId> ImGuiCompatRenderer::LoadFont(const Utility::Path& path, float sizePixels) {
    if (!Utility::File::Exists(path)) { return std::unexpected(Error::FileNotFound); }

    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.ToString().c_str(), sizePixels);
    if (font == nullptr) { return std::unexpected(Error::FontLoadFailed); }

    const std::uint32_t id = _nextFontId++;
    _fontsById.emplace(id, font);
    return Rendering::FontId(id);
}

void ImGuiCompatRenderer::Render(const Rendering::CommandBuffer& buffer) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (const Rendering::Command& command : buffer) {
        std::visit(
            [dl, this](const auto& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, Rendering::DrawRect>) {
                    Translate(dl, cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawText>) {
                    Translate(dl, cmd, ResolveFont(cmd.Font));
                } else if constexpr (std::is_same_v<T, Rendering::DrawImage>) {
                    Translate(dl, cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawPath>) {
                    Translate(dl, cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawShadow>) {
                    Translate(dl, cmd);
                } else if constexpr (std::is_same_v<T, Rendering::PushClipRect>) {
                    Translate(dl, cmd);
                } else if constexpr (std::is_same_v<T, Rendering::PopClipRect>) {
                    dl->PopClipRect();
                } else if constexpr (std::is_same_v<T, Rendering::PushOpacityLayer> ||
                                      std::is_same_v<T, Rendering::PushBlendLayer> ||
                                      std::is_same_v<T, Rendering::PopLayer>) {
                    // Structural no-op today — see file comment.
                }
            },
            command);
    }
}

} // namespace ImFrame::Internal
