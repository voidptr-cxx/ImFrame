/**
 * @file     Toast.hpp
 * @brief    Transient notification toasts with fade animation and a queue
 *
 * `ToastManager` is a process-wide singleton that manages a capped active set
 * and an overflow queue. Free functions `ToastInfo`, `ToastSuccess`,
 * `ToastWarning`, and `ToastError` delegate to `ToastManager::Instance()`.
 *
 * Call `ToastManager::Instance().Render(dt)` once per frame from within an
 * active ImGui frame (Application does this automatically from RunOneFrame).
 * Each toast fades in, holds, then fades out using an `AnimatedValue<float>`
 * for opacity.  When a toast expires it is removed and the next queued toast
 * is promoted to the active set.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ImFrame::Overlay {

// ─── ToastType ────────────────────────────────────────────────────────────────

/**
 * @brief Severity level that controls the accent colour of the toast
 *
 * @since 1.3.0
 */
enum class ToastType {
    Info,    ///< Informational (blue)
    Success, ///< Positive result (green)
    Warning, ///< Caution (amber)
    Error,   ///< Failure (red)
};

// ─── ToastConfig ──────────────────────────────────────────────────────────────

/**
 * @brief Configuration parameters for ToastManager
 *
 * Pass to `ToastManager::Configure()` before the first toast is shown.
 * Any field left at its default is safe.
 *
 * @since 1.3.0
 */
struct ToastConfig {
    int   maxVisible      = 3;    ///< Maximum simultaneously active toasts (extras queue)
    float defaultDuration = 3.0f; ///< Hold duration in seconds (override per toast with Add())
    float fadeInDuration  = 0.3f; ///< Fade-in window in seconds
    float fadeOutDuration = 0.4f; ///< Fade-out window in seconds
};

// ─── ToastSnapshot (Phase 29) ───────────────────────────────────────────────────

/**
 * @struct   ToastSnapshot
 * @brief    Read-only view of one active toast's render-relevant state
 *
 * Returned by `ToastManager::Snapshot()` for `ToastOverlayWidget` — a
 * declarative, `Portal`-based alternative to `Render()`'s raw ImGui draw-list
 * calls, without touching the fade/queue logic that already exists in
 * `ToastManager::Impl`.
 *
 * @since    2.4.0
 */
struct ToastSnapshot {
    ToastType   Type;
    std::string Title;
    std::string Body;
    float       Opacity = 1.0f; ///< Current fade-in/out opacity, `[0, 1]`.
};

// ─── ToastManager ─────────────────────────────────────────────────────────────

/**
 * @class    ToastManager
 * @brief    Singleton that drives the toast lifecycle: queue, animate, render
 *
 * At most `maxVisible` toasts are active simultaneously. Additional toasts are
 * held in a FIFO queue and promoted as active toasts expire.
 *
 * Rendering uses `ImGui::GetForegroundDrawList()` to overlay toasts above all
 * other windows without requiring a dedicated ImGui window.
 *
 * @note     Not thread-safe — call only from the render thread.
 *
 * @since    1.3.0
 *
 * @example
 * @code
 * // In application startup:
 * ImFrame::Overlay::ToastManager::Instance().Configure({ .maxVisible = 4 });
 *
 * // In UI code:
 * ImFrame::Overlay::ToastSuccess("Saved", "File written to disk.");
 * ImFrame::Overlay::ToastError("Connection lost");
 * @endcode
 */
class ToastManager {
public:
    /**
     * @brief    Returns the process-wide singleton instance
     * @return   Reference to the single ToastManager
     * @throws   Nothing — noexcept
     */
    static ToastManager& Instance() noexcept;

    /**
     * @brief    Apply configuration before any toasts are shown
     *
     * Safe to call multiple times; the new config takes effect immediately.
     *
     * @param[in]  config  New configuration to apply
     */
    void Configure(ToastConfig config);

    /**
     * @brief    Add a toast to the active set or queue it if the set is full
     *
     * @param[in]  type      Severity level
     * @param[in]  title     Short primary text shown in bold
     * @param[in]  body      Optional secondary text (empty = single-line toast)
     * @param[in]  duration  Hold duration in seconds; -1 uses `defaultDuration`
     */
    void Add(ToastType type, std::string title,
             std::string body = "", float duration = -1.0f);

    /**
     * @brief    Update state and render all active toasts
     *
     * Must be called once per frame from within an active ImGui frame.
     * `Application::RunOneFrame()` calls this automatically after `OnUi()`.
     *
     * @param[in]  dt  Delta time in seconds since the last frame
     */
    void Render(float dt);

    /**
     * @brief    Returns the number of currently active (visible) toasts
     * @return   Count in range [0, maxVisible]
     */
    [[nodiscard]] int ActiveCount() const noexcept;

    /**
     * @brief    Returns the number of toasts waiting in the overflow queue
     * @return   Queue depth (0 when all toasts fit in the active set)
     */
    [[nodiscard]] int QueuedCount() const noexcept;

    /**
     * @brief    Remove all active and queued toasts immediately
     */
    void Clear() noexcept;

    /**
     * @brief    Returns a read-only snapshot of the currently active toasts.
     *
     * For `ToastOverlayWidget` (Phase 29) — lets a declarative `Build()` render
     * the same toast queue/fade state `Render()` uses, without duplicating the
     * animation logic in `Impl`.
     *
     * @return   Active toasts in display order (oldest first).
     */
    [[nodiscard]] std::vector<ToastSnapshot> Snapshot() const;

    ToastManager(const ToastManager&)            = delete;
    ToastManager& operator=(const ToastManager&) = delete;
    ToastManager(ToastManager&&)                 = delete;
    ToastManager& operator=(ToastManager&&)      = delete;

private:
    struct Impl;

    ToastManager();
    ~ToastManager();

    std::unique_ptr<Impl> _impl;
};

// ─── Convenience free functions ───────────────────────────────────────────────

/**
 * @brief Enqueue an informational toast
 * @param[in]  title     Short primary label
 * @param[in]  body      Optional detail line
 * @param[in]  duration  Override hold duration in seconds (-1 = default)
 */
void ToastInfo(std::string title, std::string body = "", float duration = -1.0f);

/**
 * @brief Enqueue a success toast
 * @param[in]  title     Short primary label
 * @param[in]  body      Optional detail line
 * @param[in]  duration  Override hold duration in seconds (-1 = default)
 */
void ToastSuccess(std::string title, std::string body = "", float duration = -1.0f);

/**
 * @brief Enqueue a warning toast
 * @param[in]  title     Short primary label
 * @param[in]  body      Optional detail line
 * @param[in]  duration  Override hold duration in seconds (-1 = default)
 */
void ToastWarning(std::string title, std::string body = "", float duration = -1.0f);

/**
 * @brief Enqueue an error toast
 * @param[in]  title     Short primary label
 * @param[in]  body      Optional detail line
 * @param[in]  duration  Override hold duration in seconds (-1 = default)
 */
void ToastError(std::string title, std::string body = "", float duration = -1.0f);

// ─── ToastOverlayWidget (Phase 29) ──────────────────────────────────────────────

/**
 * @class    ToastOverlayWidget
 * @brief    Declarative, `Portal`-based toast renderer — a `Tree::Component`
 *
 * Stateless by design: takes a snapshot of the toasts to render (typically
 * `ToastManager::Instance().Snapshot()`, reusing its existing queue/fade
 * logic) and composes them into a bottom-right stack via `Portal`, so they
 * always render on top regardless of where this widget sits in the tree.
 * Being stateless sidesteps the "parent rebuild resets embedded `State<T>`"
 * pitfall entirely — there is no persisted state to lose.
 *
 * This coexists with the existing automatic `Application::RunOneFrame()` call
 * to `ToastManager::Instance().Render(dt)` — use one or the other, not both,
 * to avoid rendering the same toasts twice.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * struct MyRoot {
 *     Widget Build() const {
 *         return Flex(Flex::Axis::Vertical).Children({
 *             Widget(MainContent{}),
 *             Widget(ToastOverlayWidget(Overlay::ToastManager::Instance().Snapshot())),
 *         });
 *     }
 * };
 * @endcode
 */
class ToastOverlayWidget {
public:
    explicit ToastOverlayWidget(std::vector<ToastSnapshot> toasts) : _toasts(std::move(toasts)) {}

    /// @internal Composes the `Portal`-wrapped toast stack. Defined in `Toast.cpp`.
    [[nodiscard]] Tree::Widget Build() const;

private:
    std::vector<ToastSnapshot> _toasts;
};

} // namespace ImFrame::Overlay
