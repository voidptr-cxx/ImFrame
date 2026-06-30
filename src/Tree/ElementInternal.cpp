/**
 * @file     ElementInternal.cpp
 * @brief    Implementation of the shared child-reconciliation helpers
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

#include "ElementInternal.hpp"

#include <unordered_map>

using ImFrame::Tree::Element;
using ImFrame::Tree::Widget;

namespace ImFrame::Internal {

void ReconcileChildren(Element* parent,
                        std::vector<std::unique_ptr<Element>>& children,
                        const std::vector<Widget>& newWidgets) {
    std::unordered_map<std::uint64_t, std::unique_ptr<Element>> keyed;
    std::vector<std::unique_ptr<Element>>                       unkeyed;
    unkeyed.reserve(children.size());

    for (auto& child : children) {
        if (child->CurrentKey().HasValue()) {
            keyed.emplace(child->CurrentKey().Value(), std::move(child));
        } else {
            unkeyed.push_back(std::move(child));
        }
    }
    children.clear();

    std::vector<std::unique_ptr<Element>> result;
    result.reserve(newWidgets.size());
    std::size_t unkeyedCursor = 0;

    for (std::size_t i = 0; i < newWidgets.size(); ++i) {
        const Widget&             w = newWidgets[i];
        std::unique_ptr<Element> reused;

        if (w.GetKey().HasValue()) {
            auto it = keyed.find(w.GetKey().Value());
            if (it != keyed.end() && it->second->CanUpdate(w)) {
                reused = std::move(it->second);
                keyed.erase(it);
            }
        } else {
            while (unkeyedCursor < unkeyed.size() && !unkeyed[unkeyedCursor]) { ++unkeyedCursor; }
            if (unkeyedCursor < unkeyed.size() && unkeyed[unkeyedCursor]->CanUpdate(w)) {
                reused = std::move(unkeyed[unkeyedCursor]);
                ++unkeyedCursor;
            }
        }

        if (reused) {
            reused->Update(w);
            result.push_back(std::move(reused));
        } else {
            auto created = w.CreateElement();
            created->Mount(parent, i, w);
            result.push_back(std::move(created));
        }
    }

    for (auto& [key, leftover] : keyed) {
        if (leftover) { leftover->Unmount(); }
    }
    for (std::size_t i = unkeyedCursor; i < unkeyed.size(); ++i) {
        if (unkeyed[i]) { unkeyed[i]->Unmount(); }
    }

    children = std::move(result);
}

void ReconcileChild(Element* parent, std::unique_ptr<Element>& child, const Widget* newWidget) {
    if (!newWidget) {
        if (child) { child->Unmount(); }
        child.reset();
        return;
    }
    if (child && child->CanUpdate(*newWidget)) {
        child->Update(*newWidget);
    } else {
        if (child) { child->Unmount(); }
        child = newWidget->CreateElement();
        child->Mount(parent, 0, *newWidget);
    }
}

} // namespace ImFrame::Internal
