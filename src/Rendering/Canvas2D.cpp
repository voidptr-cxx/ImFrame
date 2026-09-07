/**
 * @file     Canvas2D.cpp
 * @brief    Canvas2D widget implementation: BeginChild, OnDraw dispatch, layer composite
 *
 * CanvasImpl stores per-canvas persistent state (camera, static cache, layer settings).
 * Show() creates the ImGui child window, invokes OnDraw, stable-sorts deferred commands
 * by layer index, clips and replays them into the window draw list, then fires OnHit.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Rendering/Canvas2D.hpp"
#include "DrawContextImpl.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <unordered_map>

// ─── CanvasImpl ───────────────────────────────────────────────────────────────

namespace {

struct LayerSettings {
    bool  visible = true;
    float opacity = 1.0f;
};

} // anonymous namespace

namespace ImFrame::Internal {

struct CanvasImpl {
    std::string                                         id;
    Widgets::Vec2                                       requestedSize    = {0.f, 0.f};
    bool                                                hasExplicitSize  = false;
    Widgets::Vec4                                       background       = {0.15f, 0.15f, 0.15f, 1.f};
    Rendering::CanvasInteractionMode                    interactionMode  = Rendering::CanvasInteractionMode::None;
    Utility::Delegate<void(Rendering::DrawContext&)>    onDraw;
    Utility::Delegate<void(uint32_t, Widgets::Vec2)>    onHit;
    Rendering::Camera2D                                 camera;

    // Persists across frames
    std::unordered_map<uint64_t, CachedBlock>           staticCache;
    std::unordered_map<int, LayerSettings>              layerSettings;

    // Rebuilt each frame in Show()
    std::vector<DeferredDrawCmd>                        deferred;
    std::vector<HitRecord>                              hitRecords;
    int                                                 layerCount       = 0;

    // Updated in Show() so FitToRect can use it
    Widgets::Vec2                                       lastViewportSize = {0.f, 0.f};
};

} // namespace ImFrame::Internal

// ─── Canvas2D ─────────────────────────────────────────────────────────────────

namespace ImFrame::Rendering {

Canvas2D::Canvas2D(std::string_view id)
    : _impl{std::make_unique<Internal::CanvasImpl>()}
{
    _impl->id = std::string{id};
}

Canvas2D::~Canvas2D() noexcept = default;
Canvas2D::Canvas2D(Canvas2D&&) noexcept = default;
Canvas2D& Canvas2D::operator=(Canvas2D&&) noexcept = default;

// ─── Builder setters ──────────────────────────────────────────────────────────

Canvas2D& Canvas2D::Size(Widgets::Vec2 size) {
    _impl->requestedSize   = size;
    _impl->hasExplicitSize = true;
    return *this;
}

Canvas2D& Canvas2D::Background(Widgets::Vec4 color) {
    _impl->background = color;
    return *this;
}

Canvas2D& Canvas2D::InteractionMode(CanvasInteractionMode mode) {
    _impl->interactionMode = mode;
    return *this;
}

Canvas2D& Canvas2D::OnDraw(Utility::Delegate<void(DrawContext&)> callback) {
    _impl->onDraw = std::move(callback);
    return *this;
}

Canvas2D& Canvas2D::OnHit(Utility::Delegate<void(uint32_t, Widgets::Vec2)> callback) {
    _impl->onHit = std::move(callback);
    return *this;
}

// ─── Camera ───────────────────────────────────────────────────────────────────

Canvas2D& Canvas2D::MinZoom(float minZoom) {
    _impl->camera.MinZoom(minZoom);
    return *this;
}

Canvas2D& Canvas2D::MaxZoom(float maxZoom) {
    _impl->camera.MaxZoom(maxZoom);
    return *this;
}

void Canvas2D::ResetCamera() {
    _impl->camera.Reset();
}

void Canvas2D::FitToRect(Widgets::Vec2 min, Widgets::Vec2 max) {
    const Widgets::Vec2 vp = _impl->lastViewportSize;
    if (vp.x > 0.f && vp.y > 0.f) {
        _impl->camera.FitToRect(min, max, vp);
    }
}

const Camera2D& Canvas2D::GetCamera() const noexcept {
    return _impl->camera;
}

// ─── Hit testing ──────────────────────────────────────────────────────────────

std::optional<uint32_t> Canvas2D::HitTest(Widgets::Vec2 screenPos) const {
    // Iterate in reverse so the topmost layer (highest index) is checked first.
    const auto& records = _impl->hitRecords;
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        if (screenPos.x >= it->screenMin.x && screenPos.x <= it->screenMax.x &&
            screenPos.y >= it->screenMin.y && screenPos.y <= it->screenMax.y) {
            return it->hitTargetId;
        }
    }
    return std::nullopt;
}

// ─── Layer management ─────────────────────────────────────────────────────────

void Canvas2D::SetLayerVisible(int layerIndex, bool visible) {
    _impl->layerSettings[layerIndex].visible = visible;
}

void Canvas2D::SetLayerOpacity(int layerIndex, float opacity) {
    _impl->layerSettings[layerIndex].opacity = opacity;
}

int Canvas2D::LayerCount() const noexcept {
    return _impl->layerCount;
}

// ─── Show ─────────────────────────────────────────────────────────────────────

bool Canvas2D::Show() {
    auto& im = *_impl;

    const ImVec2 size = im.hasExplicitSize
        ? ImVec2{im.requestedSize.x, im.requestedSize.y}
        : ImVec2{0.f, 0.f};

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4{im.background.x, im.background.y, im.background.z, im.background.w});

    const bool visible = ImGui::BeginChild(im.id.c_str(), size,
                                           ImGuiChildFlags_None,
                                           ImGuiWindowFlags_NoScrollbar |
                                           ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    bool hitThisFrame = false;

    if (visible) {
        const ImVec2 origin      = ImGui::GetCursorScreenPos();
        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
        im.lastViewportSize      = {contentSize.x, contentSize.y};

        // ── Camera interaction ────────────────────────────────────────────────
        if (im.interactionMode != CanvasInteractionMode::None &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) {
            ImGuiIO& io = ImGui::GetIO();

            const bool doPan =
                ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                (im.interactionMode == CanvasInteractionMode::Pan &&
                 ImGui::IsMouseDragging(ImGuiMouseButton_Left)) ||
                (im.interactionMode == CanvasInteractionMode::PanZoom &&
                 ImGui::IsMouseDragging(ImGuiMouseButton_Middle));

            if (doPan && (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f)) {
                const float z = im.camera.GetZoom();
                Widgets::Vec2 pos = im.camera.GetPosition();
                pos.x -= io.MouseDelta.x / z;
                pos.y -= io.MouseDelta.y / z;
                im.camera.SetPosition(pos);
            }

            if (im.interactionMode == CanvasInteractionMode::PanZoom && io.MouseWheel != 0.f) {
                const float factor  = 1.0f + io.MouseWheel * 0.1f;
                const Widgets::Vec2 orig{origin.x, origin.y};

                // Record canvas point under cursor before zoom change.
                const Widgets::Vec2 canvasBefore =
                    im.camera.ScreenToCanvas({io.MousePos.x, io.MousePos.y}, orig);

                im.camera.SetZoom(im.camera.GetZoom() * factor);

                // Re-derive canvas point after zoom; offset position so it stays fixed.
                const Widgets::Vec2 canvasAfter =
                    im.camera.ScreenToCanvas({io.MousePos.x, io.MousePos.y}, orig);

                Widgets::Vec2 pos = im.camera.GetPosition();
                pos.x += canvasBefore.x - canvasAfter.x;
                pos.y += canvasBefore.y - canvasAfter.y;
                im.camera.SetPosition(pos);
            }
        }

        // ── OnDraw ───────────────────────────────────────────────────────────
        im.deferred.clear();
        im.hitRecords.clear();

        if (im.onDraw) {
            Internal::DrawContextImpl dcImpl;
            dcImpl.deferred       = &im.deferred;
            dcImpl.hitRecords     = &im.hitRecords;
            dcImpl.staticCache    = &im.staticCache;
            dcImpl.camera         = &im.camera;
            dcImpl.viewportOrigin = origin;
            dcImpl.viewportSize   = contentSize;

            DrawContext dc{&dcImpl};
            im.onDraw(dc);
        }

        // ── Count unique layers ───────────────────────────────────────────────
        {
            int prev = std::numeric_limits<int>::min();
            int cnt  = 0;
            for (auto& cmd : im.deferred) {
                if (cmd.layerIndex != prev) { ++cnt; prev = cmd.layerIndex; }
            }
            im.layerCount = cnt;
        }

        // ── Composite layers ─────────────────────────────────────────────────
        if (!im.deferred.empty()) {
            std::stable_sort(im.deferred.begin(), im.deferred.end(),
                [](const DeferredDrawCmd& a, const DeferredDrawCmd& b) {
                    return a.layerIndex < b.layerIndex;
                });

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(origin,
                             {origin.x + contentSize.x, origin.y + contentSize.y},
                             true);

            for (auto& cmd : im.deferred) {
                // Per-layer visibility check.
                // Per-layer opacity is a Phase 26 concern: ImDrawList::Add* calls
                // consume pre-computed ImU32 colours and are unaffected by style vars.
                auto it = im.layerSettings.find(cmd.layerIndex);
                if (it != im.layerSettings.end() && !it->second.visible) continue;
                cmd.draw(dl);
            }

            dl->PopClipRect();
        }

        // ── Hit test on left click ────────────────────────────────────────────
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImGuiIO& io = ImGui::GetIO();
            const Widgets::Vec2 mousePos{io.MousePos.x, io.MousePos.y};
            auto hitId = HitTest(mousePos);
            if (hitId.has_value()) {
                hitThisFrame = true;
                if (im.onHit) {
                    const Widgets::Vec2 orig{origin.x, origin.y};
                    const Widgets::Vec2 cPos = im.camera.ScreenToCanvas(mousePos, orig);
                    im.onHit(*hitId, cPos);
                }
            }
        }
    }

    ImGui::EndChild();
    return hitThisFrame;
}

} // namespace ImFrame::Rendering

// ─── Canvas2DWidget / Canvas2DElement (Phase 29) ────────────────────────────────

namespace ImFrame::Internal {

class Canvas2DElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Rendering::Canvas2DWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Rendering::Canvas2DWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = {constraints.MaxWidth, constraints.MaxHeight};
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
        Rendering::Canvas2D* canvas = _config.GetCanvas();
        if (!canvas) { return; }
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        (void)canvas->Show();
    }

private:
    Rendering::Canvas2DWidget _config{nullptr};
};

} // namespace ImFrame::Internal

namespace ImFrame::Rendering {

std::unique_ptr<Tree::Element> Canvas2DWidget::CreateElement() const {
    return std::make_unique<Internal::Canvas2DElement>();
}

} // namespace ImFrame::Rendering
