/**
 * @file     ElementInternal.hpp
 * @brief    Shared child-reconciliation helpers used by every primitive's Element
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Element.hpp"
#include "ImFrame/Tree/Widget.hpp"

#include <memory>
#include <vector>

namespace ImFrame::Internal {

/**
 * @brief    Reconciles an ordered child-element list against a new ordered widget list.
 *
 * Widgets carrying an explicit `Key` are matched by key; unkeyed widgets are
 * matched structurally against the remaining unkeyed old children in order.
 * A match additionally requires `Element::CanUpdate()` (same concrete type).
 * Unmatched old children are unmounted; unmatched new widgets create new
 * elements via `Widget::CreateElement()`.
 *
 * @param[in]     parent      Owning parent element (passed to `Mount()` for newly created children).
 * @param[in,out] children    Existing child elements; replaced in place with the reconciled list.
 * @param[in]     newWidgets  Incoming widget descriptions, in order.
 */
void ReconcileChildren(Tree::Element* parent,
                        std::vector<std::unique_ptr<Tree::Element>>& children,
                        const std::vector<Tree::Widget>& newWidgets);

/**
 * @brief    Reconciles a single optional child slot.
 *
 * @param[in]     parent     Owning parent element (passed to `Mount()` if a new child is created).
 * @param[in,out] child      Existing child element (may be null); replaced in place.
 * @param[in]     newWidget  Incoming widget, or `nullptr` to clear the slot.
 */
void ReconcileChild(Tree::Element* parent,
                     std::unique_ptr<Tree::Element>& child,
                     const Tree::Widget* newWidget);

} // namespace ImFrame::Internal
