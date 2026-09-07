/**
 * @file     Viewport.cpp
 * @brief    Viewport builder, Show() compositor, and HeadlessViewport readback
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Rendering/Viewport.hpp"
#include "ImFrame/App/Application.hpp"
#include "App/ApplicationContext.hpp"
#include "Rendering/ViewportRegistry.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace ImFrame::Rendering {

// ─── Viewport ─────────────────────────────────────────────────────────────────

Viewport::Viewport(std::string_view id) : _id(id) {}

Viewport::~Viewport() noexcept {
    if (_registry) {
        _registry->Remove(_id);
    }
}

Viewport& Viewport::Size(Widgets::Vec2 size) {
    _requestedSize   = size;
    _hasExplicitSize = true;
    return *this;
}

Viewport& Viewport::OnRender(Utility::Delegate<void(const RenderContext&)> callback) {
    _onRender = std::move(callback);
    return *this;
}

Viewport& Viewport::OnResize(Utility::Delegate<void(Widgets::Vec2)> callback) {
    _onResize = std::move(callback);
    return *this;
}

Viewport& Viewport::OnInput(Utility::Delegate<void(const InputEvent&)> callback) {
    _onInput = std::move(callback);
    return *this;
}

Viewport& Viewport::Borderless(bool borderless) {
    _borderless = borderless;
    return *this;
}

void Viewport::Show() {
    App::Application* app = Internal::GetCurrentApplication();
    if (!app) return;

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const std::uint32_t w = _hasExplicitSize
        ? std::max(static_cast<std::uint32_t>(_requestedSize.x), 1u)
        : std::max(static_cast<std::uint32_t>(avail.x), 1u);
    const std::uint32_t h = _hasExplicitSize
        ? std::max(static_cast<std::uint32_t>(_requestedSize.y), 1u)
        : std::max(static_cast<std::uint32_t>(avail.y), 1u);

    Internal::ViewportEntry& entry =
        app->_viewportRegistry->GetOrCreate(this, w, h, *app->_backend);

    // Cache the registry pointer so ~Viewport() can Remove() even outside a frame.
    _registry = app->_viewportRegistry.get();

    if (entry.imTextureId != 0) {
        ImGui::Image(
            ImTextureRef{static_cast<ImTextureID>(entry.imTextureId)},
            ImVec2(static_cast<float>(entry.currentW),
                   static_cast<float>(entry.currentH)));

        const ImVec2 pos  = ImGui::GetItemRectMin();
        const ImVec2 size = ImGui::GetItemRectSize();
        app->_viewportRegistry->MarkShown(
            _id, { pos.x, pos.y, size.x, size.y });
    }
}

// ─── Internal accessors ───────────────────────────────────────────────────────

std::string_view Viewport::Id() const noexcept          { return _id; }
Widgets::Vec2    Viewport::RequestedSize() const noexcept { return _requestedSize; }
bool             Viewport::HasExplicitSize() const noexcept { return _hasExplicitSize; }
bool             Viewport::HasInputCallback() const noexcept { return bool(_onInput); }

void Viewport::FireOnRender(const RenderContext& ctx) {
    if (_onRender) _onRender(ctx);
}

void Viewport::FireOnResize(Widgets::Vec2 newSize) {
    if (_onResize) _onResize(newSize);
}

void Viewport::FireOnInput(const InputEvent& ev) {
    if (_onInput) _onInput(ev);
}

// ─── HeadlessViewport ─────────────────────────────────────────────────────────

std::expected<std::vector<std::byte>, Core::Error> HeadlessViewport::ReadPixels() const {
    if (_lastPixels.empty()) {
        return std::unexpected(Core::Error::NotInitialised);
    }
    return _lastPixels;
}

void HeadlessViewport::CaptureAfterRender(const Internal::ViewportHandles& h) {
    if (!h.headlessPixels || h.width == 0 || h.height == 0) return;
    const std::size_t byteCount =
        static_cast<std::size_t>(h.width) * h.height * 4u;
    _lastPixels.resize(byteCount);
    std::memcpy(_lastPixels.data(),
                reinterpret_cast<const std::byte*>(h.headlessPixels),
                byteCount);
}

} // namespace ImFrame::Rendering

// ─── ViewportWidget / ViewportElement (Phase 29) ────────────────────────────────

namespace ImFrame::Internal {

class ViewportElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Rendering::ViewportWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Rendering::ViewportWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        Rendering::Viewport* vp = _config.GetViewport();
        if (vp && vp->HasExplicitSize()) {
            _size = constraints.Constrain(vp->RequestedSize());
        } else {
            _size = {constraints.MaxWidth, constraints.MaxHeight};
        }
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
        Rendering::Viewport* vp = _config.GetViewport();
        if (!vp) { return; }
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        vp->Show();
    }

private:
    Rendering::ViewportWidget _config{nullptr};
};

} // namespace ImFrame::Internal

namespace ImFrame::Rendering {

std::unique_ptr<Tree::Element> ViewportWidget::CreateElement() const {
    return std::make_unique<Internal::ViewportElement>();
}

} // namespace ImFrame::Rendering
