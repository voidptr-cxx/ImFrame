/**
 * @file     Checkbox.hpp
 * @brief    Boolean toggle checkbox widget
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string>

namespace ImFrame::Widgets {

// ─── CheckboxWidget (Phase 29) ──────────────────────────────────────────────────

/**
 * @class    CheckboxWidget
 * @brief    Declarative checkbox — `Tree::PrimitiveWidget` replacement for `Checkbox`
 *
 * Binds to caller-owned storage via a raw pointer (not a reference — reference
 * members would make this type non-copy-assignable, breaking the
 * `_config = widget.As<T>()` pattern every primitive `Element` relies on).
 * Produces the exact same `ImGui::Checkbox()` call as `Checkbox::Show()`.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * bool wireframe = false;
 * CheckboxWidget("Wireframe", &wireframe).OnChange([&](bool v) { renderer.SetWireframe(v); });
 * @endcode
 */
class CheckboxWidget {
public:
    /// `value` must outlive this widget and every `Element` mounted from it.
    explicit CheckboxWidget(std::string label, bool* value) : _label(std::move(label)), _value(value) {}

    CheckboxWidget& OnChange(Utility::Delegate<void(bool)> cb) { _onChange = std::move(cb); return *this; }
    CheckboxWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    CheckboxWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    CheckboxWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    CheckboxWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] bool* GetValue() const noexcept { return _value; }
    [[nodiscard]] const Utility::Delegate<void(bool)>& GetOnChange() const noexcept { return _onChange; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    /// @internal Produces this checkbox's concrete `Element`. Defined in `Checkbox.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    std::string                    _label;
    bool*                          _value = nullptr;
    Utility::Delegate<void(bool)>  _onChange;
    bool                           _disabled = false;
    std::string                    _tooltip;
    float                          _width = 0.0f;
    Tree::Key                      _key;
};

} // namespace ImFrame::Widgets
