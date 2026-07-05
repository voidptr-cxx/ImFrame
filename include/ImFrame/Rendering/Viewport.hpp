/**
 * @file     Viewport.hpp
 * @brief    Offscreen framebuffer widget composited into the ImGui frame as a texture
 *
 * `Viewport` follows the Phase 10–14 builder pattern: construct, set options via
 * fluent setters, then call `Show()` once per frame from the `OnUi` callback. The
 * `OnRender` callback is invoked by `Application` before `BeginFrame()` so the
 * framebuffer is ready when ImGui samples it via `ImGui::Image()`.
 *
 * `HeadlessViewport` extends `Viewport` with `ReadPixels()` for use in automated
 * tests against the headless backend.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/InputEvent.hpp"
#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Rendering/RenderContext.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

namespace ImFrame::Internal { class ViewportRegistry; }

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace ImFrame::Rendering {

/**
 * @class    Viewport
 * @brief    Offscreen framebuffer widget composited into the parent ImGui window as a texture
 *
 * Each frame: `DispatchViewportRenders()` calls `OnRender` to fill the framebuffer,
 * then `Show()` composites it via `ImGui::Image()`. Rendering happens before the
 * ImGui pass begins — the two calls are not interleaved.
 *
 * Two Viewports with the same ID in the same `Application` share one framebuffer.
 * This is documented behaviour, not a bug: it lets multi-panel tools display the
 * same render output in two locations.
 *
 * @note     Non-copyable. Move is allowed before first `Show()`.
 *
 * @since    2.0.0
 *
 * @example
 * @code
 * // In OnUi — called every frame:
 * Rendering::Viewport("3d_view")
 *     .OnRender([](const Rendering::RenderContext& ctx) {
 *         auto& gl = std::get<Rendering::ViewportImageGL>(ctx.NativeImage);
 *         glBindFramebuffer(GL_FRAMEBUFFER, gl.Framebuffer);
 *         // ... render scene ...
 *         glBindFramebuffer(GL_FRAMEBUFFER, 0);
 *     })
 *     .Show();
 * @endcode
 */
class Viewport {
public:
    /**
     * @brief    Construct a Viewport with the given stable ID.
     *
     * @param[in]  id  Unique identifier used for framebuffer lifetime management.
     *                 Must be stable across frames for the same logical Viewport.
     */
    explicit Viewport(std::string_view id);

    /**
     * @brief  Destructor — unregisters the Viewport from the Application.
     */
    virtual ~Viewport() noexcept;

    Viewport(const Viewport&)            = delete;
    Viewport& operator=(const Viewport&) = delete;
    Viewport(Viewport&&)                 = default;
    Viewport& operator=(Viewport&&)      = default;

    // ─── Builder setters ──────────────────────────────────────────────────────

    /**
     * @brief    Pin the Viewport to a fixed pixel size.
     *
     * When not called, the Viewport expands to fill the available content region
     * of its parent ImGui window.
     *
     * @param[in]  size  Desired pixel dimensions. Both components must be > 0.
     * @return   Reference to this Viewport for chaining.
     */
    Viewport& Size(Widgets::Vec2 size);

    /**
     * @brief    Register the render callback invoked before each ImGui frame.
     *
     * The callback is called by `DispatchViewportRenders()` before `BeginFrame()`.
     * All GPU commands that write into `ctx.NativeImage` must be submitted or
     * recorded before the callback returns.
     *
     * @param[in]  callback  Callable accepting a `const RenderContext&`.
     * @return   Reference to this Viewport for chaining.
     */
    Viewport& OnRender(Utility::Delegate<void(const RenderContext&)> callback);

    /**
     * @brief    Register a callback fired when the Viewport's pixel dimensions change.
     *
     * @param[in]  callback  Callable accepting the new `Vec2` pixel size.
     * @return   Reference to this Viewport for chaining.
     */
    Viewport& OnResize(Utility::Delegate<void(Widgets::Vec2)> callback);

    /**
     * @brief    Register an input-forwarding callback.
     *
     * When set, `InputEvent` values whose screen position falls within the
     * Viewport's screen rectangle are delivered here instead of to ImGui.
     * Key events are forwarded to the Viewport that last received a mouse click.
     *
     * @param[in]  callback  Callable accepting a `const InputEvent&`.
     * @return   Reference to this Viewport for chaining.
     */
    Viewport& OnInput(Utility::Delegate<void(const InputEvent&)> callback);

    /**
     * @brief    Remove the ImGui child-window border around the Viewport.
     *
     * @param[in]  borderless  `true` to remove the border. Default: `true`.
     * @return   Reference to this Viewport for chaining.
     */
    Viewport& Borderless(bool borderless = true);

    // ─── Terminal ─────────────────────────────────────────────────────────────

    /**
     * @brief    Composite this Viewport into the current ImGui window.
     *
     * Registers with the Application on first call. Issues `ImGui::Image()` using
     * the framebuffer already filled by `DispatchViewportRenders()`. Must be called
     * from the `OnUi` callback — never from `OnRender`.
     */
    void Show();

    // ─── Internal — not part of the public API ────────────────────────────────

    [[nodiscard]] std::string_view Id() const noexcept;
    [[nodiscard]] Widgets::Vec2    RequestedSize() const noexcept;
    [[nodiscard]] bool             HasExplicitSize() const noexcept;
    [[nodiscard]] bool             HasInputCallback() const noexcept;

    void FireOnRender(const RenderContext& ctx);
    void FireOnResize(Widgets::Vec2 newSize);
    void FireOnInput(const InputEvent& ev);

    /// Called by ViewportRegistry after OnRender to let subclasses capture pixel data.
    virtual void CaptureAfterRender(const Internal::ViewportHandles&) {}

protected:
    std::string _id;

private:
    Widgets::Vec2                                         _requestedSize   = {0.0f, 0.0f};
    bool                                                  _hasExplicitSize = false;
    bool                                                  _borderless      = false;
    bool                                                  _registered      = false;
    Internal::ViewportRegistry*                           _registry        = nullptr;
    Utility::Delegate<void(const RenderContext&)>         _onRender;
    Utility::Delegate<void(Widgets::Vec2)>                _onResize;
    Utility::Delegate<void(const InputEvent&)>  _onInput;
};

/**
 * @class    HeadlessViewport
 * @brief    Viewport with CPU pixel readback, for use in headless-backend test suites
 *
 * Pair with `Application::CreateHeadless()`. After at least one frame where the
 * `OnRender` callback writes to `ViewportImageHeadless::Pixels`, call `ReadPixels()`
 * to retrieve a copy of the rendered data.
 *
 * @since    2.0.0
 *
 * @example
 * @code
 * auto app = Application::CreateHeadless();
 * HeadlessViewport vp("test");
 * vp.OnRender([](const RenderContext& ctx) {
 *     auto& hl = std::get<ViewportImageHeadless>(ctx.NativeImage);
 *     std::fill(hl.Pixels, hl.Pixels + hl.Width * hl.Height * 4, 0xFF);
 * });
 * app.OnUi([&]{ vp.Show(); }).RunOneFrame();
 * auto pixels = vp.ReadPixels().value();
 * @endcode
 */
class HeadlessViewport : public Viewport {
public:
    using Viewport::Viewport;

    /**
     * @brief    Return the last rendered pixel data as a tightly-packed RGBA8 buffer.
     *
     * @return   Pixel data on success; `Core::Error::NotReady` if no frame has been
     *           rendered yet (i.e., `OnRender` has not fired at least once).
     */
    [[nodiscard]] std::expected<std::vector<std::byte>, Core::Error> ReadPixels() const;

    void CaptureAfterRender(const Internal::ViewportHandles& h) override;

private:
    std::vector<std::byte> _lastPixels;
};

// ─── ViewportWidget (Phase 29) ───────────────────────────────────────────────────

/**
 * @class    ViewportWidget
 * @brief    Declarative leaf — `Tree::PrimitiveWidget` wrapper that calls `Viewport::Show()`
 *
 * `Viewport` itself still owns the framebuffer and registers with
 * `Internal::ViewportRegistry` exactly as before — this widget only changes
 * how it is *declared* (inside a `Build()` tree instead of an imperative
 * `OnUi` call site). Binds via a raw pointer since `Viewport` is
 * non-copyable/non-copy-assignable and `ComponentElement`/primitive
 * `Element`s require their config type to be copy-assignable.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * Rendering::Viewport scene("3d_view");
 * scene.OnRender([](const RenderContext& ctx) { ... });
 * // In Build():
 * return Widget(ViewportWidget(&scene));
 * @endcode
 */
class ViewportWidget {
public:
    /// `viewport` must outlive this widget and every `Element` mounted from it.
    explicit ViewportWidget(Viewport* viewport) : _viewport(viewport) {}

    /// Explicit identity override — see `Tree::Key`.
    ViewportWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] Viewport* GetViewport() const noexcept { return _viewport; }

    /// @internal Produces this widget's concrete `Element`. Defined in `Viewport.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    Viewport* _viewport = nullptr;
    Tree::Key _key;
};

} // namespace ImFrame::Rendering
