/**
 * @file     Toast.cpp
 * @brief    ToastManager implementation: queue, fade animation, and foreground rendering
 *
 * @internal
 * Each Toast carries an `AnimatedValue<float>` for opacity driven by a simple
 * state machine: fade-in → hold → fade-out → remove.  When a toast expires it
 * is erased from the active vector and the oldest queued toast is promoted.
 *
 * Rendering uses `ImGui::GetForegroundDrawList()` to draw directly on the
 * viewport overlay — no ImGui window is needed.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Overlay/Toast.hpp"
#include "ImFrame/Anim/AnimatedValue.hpp"
#include "ImFrame/Tree/Portal.hpp"
#include "ImFrame/Tree/Primitives/Box.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <deque>
#include <vector>

namespace ImFrame::Overlay {

namespace {

// Accent colours per toast type: RGBA
constexpr ImVec4 k_toastColors[] = {
    { 0.20f, 0.52f, 0.80f, 1.0f }, // Info    — steel blue
    { 0.18f, 0.65f, 0.32f, 1.0f }, // Success — green
    { 0.85f, 0.62f, 0.12f, 1.0f }, // Warning — amber
    { 0.80f, 0.22f, 0.22f, 1.0f }, // Error   — red
};

constexpr float TOAST_W   = 300.0f;
constexpr float TOAST_H   = 44.0f;
constexpr float TOAST_H_B = 64.0f; // with body line
constexpr float TOAST_PAD = 10.0f;
constexpr float TOAST_R   = 6.0f;  // corner radius
constexpr float OPACITY_DECAY = 10.0f;

} // anonymous namespace

// ─── Internal Toast struct ────────────────────────────────────────────────────

struct Toast {
    ToastType              type;
    std::string            title;
    std::string            body;
    float                  duration;    // hold time in seconds
    float                  elapsed  = 0.0f;
    bool                   fadingOut = false;
    Anim::AnimatedValue<float> opacity{ 0.0f, OPACITY_DECAY };
};

// ─── ToastManager::Impl ───────────────────────────────────────────────────────

struct ToastManager::Impl {
    ToastConfig        config;
    std::vector<Toast> active;
    std::deque<Toast>  queue;

    void Promote() {
        while (static_cast<int>(active.size()) < config.maxVisible && !queue.empty()) {
            active.push_back(std::move(queue.front()));
            queue.pop_front();
            active.back().opacity.SetTarget(1.0f);
        }
    }
};

// ─── ToastManager ─────────────────────────────────────────────────────────────

ToastManager::ToastManager() : _impl(std::make_unique<Impl>()) {}
ToastManager::~ToastManager() = default;

ToastManager& ToastManager::Instance() noexcept {
    static ToastManager instance;
    return instance;
}

void ToastManager::Configure(ToastConfig config) {
    _impl->config = config;
}

void ToastManager::Add(ToastType type, std::string title, std::string body, float duration) {
    const float hold = (duration < 0.0f) ? _impl->config.defaultDuration : duration;

    Toast t;
    t.type     = type;
    t.title    = std::move(title);
    t.body     = std::move(body);
    t.duration = hold;

    if (static_cast<int>(_impl->active.size()) < _impl->config.maxVisible) {
        t.opacity.SetTarget(1.0f);
        _impl->active.push_back(std::move(t));
    } else {
        _impl->queue.push_back(std::move(t));
    }
}

void ToastManager::Render(float dt) {
    auto& a = _impl->active;
    const ToastConfig& cfg = _impl->config;

    // ── Update each active toast ──────────────────────────────────────────────
    for (auto& t : a) {
        t.elapsed += dt;

        if (!t.fadingOut) {
            // Trigger fade-out when hold period ends
            const float holdEnd = t.duration - cfg.fadeOutDuration;
            if (t.elapsed >= holdEnd) {
                t.fadingOut = true;
                t.opacity.SetTarget(0.0f);
            }
        }
        t.opacity.Update(dt);
    }

    // ── Remove expired toasts (fading out and fully transparent) ─────────────
    std::erase_if(a, [](const Toast& t) {
        return t.fadingOut && t.opacity.Value() < 0.01f;
    });

    // ── Promote queued toasts into the active set ─────────────────────────────
    _impl->Promote();

    if (a.empty()) {
        return;
    }

    // ── Render using foreground draw list ─────────────────────────────────────
    ImGuiViewport* vp   = ImGui::GetMainViewport();
    const float    vpR  = vp->WorkPos.x + vp->WorkSize.x;
    const float    vpB  = vp->WorkPos.y + vp->WorkSize.y;

    ImDrawList* dl = ImGui::GetForegroundDrawList(vp);

    // Render newest-first (stack grows upward from bottom-right)
    float yBottom = vpB - TOAST_PAD;

    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
        const Toast& t      = a[i];
        const float  op     = t.opacity.Value();
        const bool   hasBody = !t.body.empty();
        const float  h      = hasBody ? TOAST_H_B : TOAST_H;

        const float  xR = vpR - TOAST_PAD;
        const float  xL = xR - TOAST_W;
        const float  yT = yBottom - h;

        const ImVec4& col  = k_toastColors[static_cast<int>(t.type)];
        const ImU32   bg   = ImGui::ColorConvertFloat4ToU32({ col.x, col.y, col.z, op * 0.88f });
        const ImU32   text = ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 1.0f, op });
        const ImU32   sub  = ImGui::ColorConvertFloat4ToU32({ 0.90f, 0.90f, 0.90f, op * 0.75f });

        dl->AddRectFilled({ xL, yT }, { xR, yBottom }, bg, TOAST_R);

        // Title
        dl->AddText({ xL + 10.0f, yT + 10.0f }, text, t.title.c_str());

        // Body (second line)
        if (hasBody) {
            dl->AddText({ xL + 10.0f, yT + 28.0f }, sub, t.body.c_str());
        }

        yBottom = yT - TOAST_PAD;
    }
}

std::vector<ToastSnapshot> ToastManager::Snapshot() const {
    std::vector<ToastSnapshot> result;
    result.reserve(_impl->active.size());
    for (const Toast& t : _impl->active) {
        result.push_back(ToastSnapshot{t.type, t.title, t.body, t.opacity.Value()});
    }
    return result;
}

int ToastManager::ActiveCount() const noexcept {
    return static_cast<int>(_impl->active.size());
}

int ToastManager::QueuedCount() const noexcept {
    return static_cast<int>(_impl->queue.size());
}

void ToastManager::Clear() noexcept {
    _impl->active.clear();
    _impl->queue.clear();
}

// ─── Free functions ───────────────────────────────────────────────────────────

void ToastInfo(std::string title, std::string body, float duration) {
    ToastManager::Instance().Add(ToastType::Info, std::move(title), std::move(body), duration);
}

void ToastSuccess(std::string title, std::string body, float duration) {
    ToastManager::Instance().Add(ToastType::Success, std::move(title), std::move(body), duration);
}

void ToastWarning(std::string title, std::string body, float duration) {
    ToastManager::Instance().Add(ToastType::Warning, std::move(title), std::move(body), duration);
}

void ToastError(std::string title, std::string body, float duration) {
    ToastManager::Instance().Add(ToastType::Error, std::move(title), std::move(body), duration);
}

// ─── ToastOverlayWidget (Phase 29) ──────────────────────────────────────────────

Tree::Widget ToastOverlayWidget::Build() const {
    using Tree::Primitives::Box;
    using Tree::Primitives::Flex;
    using Tree::Primitives::Text;

    std::vector<Tree::Widget> toastBoxes;
    toastBoxes.reserve(_toasts.size());
    for (const ToastSnapshot& t : _toasts) {
        const ImVec4&        col = k_toastColors[static_cast<int>(t.Type)];
        const Widgets::Vec4  bg{col.x, col.y, col.z, t.Opacity * 0.88f};
        const Widgets::Vec4  textColor{1.0f, 1.0f, 1.0f, t.Opacity};

        std::vector<Tree::Widget> lines;
        lines.push_back(Tree::Widget(Text(t.Title).Color(textColor)));
        if (!t.Body.empty()) {
            const Widgets::Vec4 subColor{0.90f, 0.90f, 0.90f, t.Opacity * 0.75f};
            lines.push_back(Tree::Widget(Text(t.Body).Color(subColor)));
        }

        toastBoxes.push_back(Tree::Widget(Box()
                                               .Width(TOAST_W)
                                               .Padding(Widgets::EdgeInsets::All(TOAST_PAD))
                                               .Background(bg)
                                               .Radius(TOAST_R)
                                               .Child(Tree::Widget(Flex(Flex::Axis::Vertical).Children(std::move(lines))))));
    }

    Tree::Widget stack = Tree::Widget(Flex(Flex::Axis::Vertical)
                                           .MainAlign(Flex::MainAlignment::End)
                                           .CrossAlign(Flex::CrossAlignment::End)
                                           .Gap(TOAST_PAD)
                                           .Children(std::move(toastBoxes)));

    return Tree::Widget(Tree::Portal(stack));
}

} // namespace ImFrame::Overlay
