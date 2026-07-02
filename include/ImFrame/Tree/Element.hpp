/**
 * @file     Element.hpp
 * @brief    Abstract live-tree node produced from a `Widget` description
 *
 * `Element` is the stateful counterpart to the immutable `Widget` value type.
 * Elements persist across frames; the reconciler updates an existing element
 * in place when a new `Widget` of the same concrete type arrives at its
 * position, or destroys and replaces it when the type changes.
 *
 * This header intentionally exposes no `RenderObject` type — layout and ImGui
 * call emission are reached only through `Layout()` / `Paint()`. Concrete
 * `Element` subclasses that own a `RenderObject`-equivalent live in
 * `ImFrame::Internal::`, defined in `src/Tree/RenderObjects/*.cpp`, which is
 * the only code in the project permitted to call raw ImGui functions after
 * this phase.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Key.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <cstddef>
#include <limits>
#include <typeindex>

namespace ImFrame::Tree {

class Widget; // Defined in Widget.hpp; CanUpdate()/RecordWidgetMeta() are defined there.

// ─── BoxConstraints ───────────────────────────────────────────────────────────

/**
 * @struct   BoxConstraints
 * @brief    Min/max width and height passed down during a `Layout()` pass
 *
 * A parent `Element` narrows constraints for each child; the child returns a
 * concrete size within those bounds from `Layout()`. Modelled on Flutter's
 * `BoxConstraints` — the same single-pass, parent-narrows / child-returns-size
 * protocol.
 *
 * @since    2.2.0
 */
struct BoxConstraints {
    float MinWidth  = 0.0f;                              ///< Minimum width, inclusive.
    float MaxWidth  = std::numeric_limits<float>::max(); ///< Maximum width, inclusive.
    float MinHeight = 0.0f;                              ///< Minimum height, inclusive.
    float MaxHeight = std::numeric_limits<float>::max(); ///< Maximum height, inclusive.

    /// Constraints that force an exact size (min == max on both axes).
    [[nodiscard]] static constexpr BoxConstraints Tight(Widgets::Vec2 size) noexcept {
        return {size.x, size.x, size.y, size.y};
    }

    /// Constraints with zero minimums and the given maximums.
    [[nodiscard]] static constexpr BoxConstraints Loose(Widgets::Vec2 maxSize) noexcept {
        return {0.0f, maxSize.x, 0.0f, maxSize.y};
    }

    [[nodiscard]] constexpr float ConstrainWidth(float w) const noexcept {
        return w < MinWidth ? MinWidth : (w > MaxWidth ? MaxWidth : w);
    }

    [[nodiscard]] constexpr float ConstrainHeight(float h) const noexcept {
        return h < MinHeight ? MinHeight : (h > MaxHeight ? MaxHeight : h);
    }

    [[nodiscard]] constexpr Widgets::Vec2 Constrain(Widgets::Vec2 size) const noexcept {
        return {ConstrainWidth(size.x), ConstrainHeight(size.y)};
    }
};

// ─── Element ──────────────────────────────────────────────────────────────────

/**
 * @class    Element
 * @brief    Abstract base for every live node in the widget tree
 *
 * @since    2.2.0
 */
class Element {
public:
    virtual ~Element() = default;

    /**
     * @brief    First-time attachment of this element into the tree.
     * @param[in] parent     Owning parent element, or `nullptr` for the root.
     * @param[in] slotIndex  This element's structural index within its parent's children.
     * @param[in] widget     The widget description this element was created from.
     */
    virtual void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) = 0;

    /**
     * @brief    Reconfigure this element from a new widget of the same concrete type.
     * @param[in] newWidget  Replacement widget description.
     */
    virtual void Update(const Widget& newWidget) = 0;

    /// Detach this element and release any owned children. Default: no-op.
    virtual void Unmount() {}

    /**
     * @brief    Measure this element (and its subtree) given parent-supplied constraints.
     * @param[in] constraints  Min/max width and height.
     * @return   This element's resulting size, within `constraints`.
     */
    [[nodiscard]] virtual Widgets::Vec2 Layout(BoxConstraints constraints) = 0;

    /**
     * @brief    Emit ImGui draw calls / cursor placement for this element's subtree.
     * @param[in] position  Absolute screen position for this element's top-left corner.
     */
    virtual void Paint(Widgets::Vec2 position) = 0;

    /**
     * @brief    Whether `newWidget` can reconfigure this element in place.
     *
     * `true` when `newWidget`'s concrete type matches the type this element
     * was last mounted/updated from. Defined in `Widget.hpp` (needs `Widget`
     * complete).
     *
     * @param[in] newWidget  Candidate replacement widget.
     */
    [[nodiscard]] bool CanUpdate(const Widget& newWidget) const noexcept;

    /// The `Key` recorded from the widget this element was last mounted/updated from.
    [[nodiscard]] Key CurrentKey() const noexcept { return _currentKey; }

    /// The size computed by the most recent `Layout()` call.
    [[nodiscard]] Widgets::Vec2 Size() const noexcept { return _size; }

protected:
    /// Records `widget`'s type and key onto this element. Call from `Mount()`/`Update()` overrides.
    void RecordWidgetMeta(const Widget& widget) noexcept;

    Element*      _parent    = nullptr;
    std::size_t   _slotIndex = 0;
    Widgets::Vec2 _size{};

private:
    std::type_index _widgetType = typeid(void);
    Key              _currentKey;
};

} // namespace ImFrame::Tree
