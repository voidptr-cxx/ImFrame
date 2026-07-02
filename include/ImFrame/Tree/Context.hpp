/**
 * @file     Context.hpp
 * @brief    Placeholder build-context type — extended in Phase 28
 *
 * Phase 27's `Component::Build()` is intentionally zero-argument (see
 * `Component.hpp`). `Context` exists in Phase 27 only as a forward-declared
 * extension point so that Phase 28 can introduce `Context::Of<T>()` for
 * `InheritedWidget` lookup without an ABI-breaking rename. It carries no data
 * yet and is not threaded into `Build()` calls in this phase.
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

namespace ImFrame::Tree {

/**
 * @class    Context
 * @brief    Empty placeholder for the Phase 28 inherited-value lookup mechanism
 *
 * @since    2.2.0
 */
class Context {
public:
    Context() noexcept = default;
};

} // namespace ImFrame::Tree
