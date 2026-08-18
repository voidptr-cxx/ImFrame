/**
 * @file     Image.cpp
 * @brief    Implementation of ImageWidget's Element
 *
 * @internal
 * ImGui 1.92+ uses ImTextureRef (wrapping ImTextureID = ImU64) for all texture
 * parameters. TextureHandle (void*) is converted via intptr_t to preserve the
 * pointer value on both 32-bit and 64-bit targets.
 *
 * `Paint()` splits on whether `OnClick` is set (Phase 32.8):
 * - Set: unchanged from Phase 30 — `ImGui::ImageButton()` handles hit-testing
 *   and drawing as one opaque call, exactly like `ButtonWidget`/`CheckboxWidget`
 *   (see `.claude/DECISIONS.md`, Phase 31.2's "interaction with no generic
 *   command equivalent" carve-out). Still calls ImGui directly.
 * - Unset: pure display, no interaction needed — pushes a real
 *   `Rendering::DrawImage` command instead of calling `ImGui::Image()`
 *   directly. `ImGuiCompatRenderer` already translates `DrawImage` to
 *   `AddImage`/`AddImageRounded` (built speculatively in Phase 31, unused
 *   until now). `TextureId`'s value is the same reinterpreted `ImTextureID`
 *   the old `ImGui::Image()` call used — matches `TextureId`'s own documented
 *   "each renderer gives this value its own meaning; ImGuiCompatRenderer
 *   treats it as an ImTextureID" contract.
 *
 * Two Phase-30 behaviours needed explicit preservation across that split,
 * since bypassing `ImGui::Image()` also bypasses what it did for free:
 * - Tooltip: `ImGui::IsItemHovered()` needs a registered ImGui item at the
 *   image's rect. `DrawImage` registers nothing, so an invisible
 *   `ImGui::Dummy()` at the same rect stands in for hover-testing only when a
 *   tooltip is actually configured — the one piece of behaviour this path
 *   still needs from ImGui.
 * - Disabled dimming: `ImGui::Image()` multiplies its tint by the ambient
 *   `ImGuiStyle::Alpha`, which `BeginDisabled()` lowers. `DrawImage` has no
 *   such ambient-alpha concept (matching every other Phase 31/32
 *   `CommandBuffer`-based element — `Box`/`Text` don't apply it either), so
 *   the same multiplier is read via a paired `BeginDisabled()`/`EndDisabled()`
 *   and baked into the pushed command's `TintColor` directly.
 * `BorderColor` has no `DrawRect`-less equivalent in `DrawImage` (unlike
 * `ImGui::Image()`'s `border_col` parameter, which also pads the image
 * outward by `ImGuiStyle::ImageBorderSize` — a detail this path does not
 * replicate pixel-for-pixel): a non-transparent border now draws as a
 * separate stroke-only `DrawRect` at the image's own bounds, undocumented by
 * any test today and not a currently-tested feature.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Image.hpp"

#include <imgui.h>

#include <cstdint>

// ─── ImageWidget / ImageElement (Phase 30) ──────────────────────────────────────

namespace ImFrame::Internal {

namespace {

[[nodiscard]] Rendering::TextureId ToTextureId(Widgets::TextureHandle texture) noexcept {
    return Rendering::TextureId(static_cast<std::uint64_t>(reinterpret_cast<uintptr_t>(texture)));
}

} // namespace

class ImageElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::ImageWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::ImageWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = constraints.Constrain(_config.GetSize());
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        ImGui::PushID(this);

        if (_config.GetOnClick()) {
            PaintInteractive(position);
        } else {
            PaintStatic(cmd, position);
        }

        ImGui::PopID();
    }

private:
    void PaintInteractive(Widgets::Vec2 position) {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});

        const auto texture = _config.GetTexture();
        const ImTextureRef texRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture)));
        const Widgets::Vec2 sizeF = _size;
        const Widgets::Vec2 uv0F  = _config.GetUV0();
        const Widgets::Vec2 uv1F  = _config.GetUV1();
        const Widgets::Vec4 tintF = _config.GetTint();
        const ImVec2 sz  {sizeF.x, sizeF.y};
        const ImVec2 uv0 {uv0F.x,  uv0F.y};
        const ImVec2 uv1 {uv1F.x,  uv1F.y};
        const ImVec4 tint{tintF.x, tintF.y, tintF.z, tintF.w};

        if (_config.GetDisabled()) { ImGui::BeginDisabled(); }
        const ImVec4 bg{0.0f, 0.0f, 0.0f, 0.0f};
        const bool clicked = ImGui::ImageButton("##img", texRef, sz, uv0, uv1, bg, tint);
        if (_config.GetDisabled()) { ImGui::EndDisabled(); }

        if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
        }

        if (clicked) { _config.GetOnClick()(); }
    }

    void PaintStatic(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) {
        const Widgets::Vec2 sizeF = _size;
        Widgets::Vec4       tintF = _config.GetTint();

        if (_config.GetDisabled()) {
            ImGui::BeginDisabled();
            tintF.w *= ImGui::GetStyle().Alpha; // see this file's header comment
            ImGui::EndDisabled();
        }

        cmd.Push(Rendering::DrawImage{
            .Position  = position,
            .Size      = sizeF,
            .Texture   = ToTextureId(_config.GetTexture()),
            .UvMin     = _config.GetUV0(),
            .UvMax     = _config.GetUV1(),
            .TintColor = tintF,
        });

        const Widgets::Vec4 borderF = _config.GetBorderColor();
        if (borderF.w > 0.0f) {
            cmd.Push(Rendering::DrawRect{
                .Position    = position,
                .Size        = sizeF,
                .StrokeColor = borderF,
                .StrokeWidth = 1.0f,
            });
        }

        if (!_config.GetTooltip().empty()) {
            ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
            ImGui::Dummy(ImVec2{sizeF.x, sizeF.y});
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
            }
        }
    }

    Widgets::ImageWidget _config{nullptr, Widgets::Vec2{}};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> ImageWidget::CreateElement() const {
    return std::make_unique<Internal::ImageElement>();
}

} // namespace ImFrame::Widgets
