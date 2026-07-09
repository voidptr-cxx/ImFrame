/**
 * @file     ImFrame.hpp
 * @brief    Umbrella include for the entire ImFrame public API
 *
 * Include this single header to access all ImFrame types and functions.
 * Individual sub-headers may be included directly for faster compile times
 * once only specific subsystems are needed.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

// ─── Core ─────────────────────────────────────────────────────────────────────
#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Core/Context.hpp"
#include "ImFrame/Core/Types.hpp"

// ─── Backends ─────────────────────────────────────────────────────────────────
#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Backends/FrameInfo.hpp"
#include "ImFrame/Backends/InputEvent.hpp"

// ─── Utility ──────────────────────────────────────────────────────────────────
#include "ImFrame/Utility/Path.hpp"
#include "ImFrame/Utility/File.hpp"
#include "ImFrame/Utility/Directory.hpp"
#include "ImFrame/Utility/FileWatcher.hpp"
#include "ImFrame/Utility/Thread.hpp"
#include "ImFrame/Utility/ThreadPool.hpp"
#include "ImFrame/Utility/BackgroundWorker.hpp"
#include "ImFrame/Utility/EventBus.hpp"
#include "ImFrame/Utility/Signal.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Utility/Timer.hpp"
#include "ImFrame/Utility/Logger.hpp"
#include "ImFrame/Utility/Config.hpp"

// ─── App ──────────────────────────────────────────────────────────────────────
#include "ImFrame/App/Application.hpp"
#include "ImFrame/App/Window.hpp"
#include "ImFrame/App/DockSpace.hpp"

// ─── Theme ────────────────────────────────────────────────────────────────────
#include "ImFrame/Theme/Theme.hpp"
#include "ImFrame/Theme/ColorToken.hpp"
#include "ImFrame/Theme/ThemeBuilder.hpp"
#include "ImFrame/Theme/Themes/Dracula.hpp"
#include "ImFrame/Theme/Themes/Nord.hpp"
#include "ImFrame/Theme/Themes/CatppuccinMocha.hpp"
#include "ImFrame/Theme/Themes/Light.hpp"

// ─── Icons ────────────────────────────────────────────────────────────────────
#include "ImFrame/Icons/Icons.hpp"
#include "ImFrame/Icons/IconFont.hpp"

// ─── Widgets ──────────────────────────────────────────────────────────────────
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/TextInput.hpp"
#include "ImFrame/Widgets/Slider.hpp"
#include "ImFrame/Widgets/Combo.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/Radio.hpp"
#include "ImFrame/Widgets/ColorEdit.hpp"
#include "ImFrame/Widgets/Image.hpp"
#include "ImFrame/Widgets/ProgressBar.hpp"
#include "ImFrame/Widgets/Separator.hpp"
#include "ImFrame/Widgets/Table.hpp"
#include "ImFrame/Widgets/PropertyGrid.hpp"
#include "ImFrame/Widgets/PlotContext.hpp"
#include "ImFrame/Widgets/LinePlot.hpp"
#include "ImFrame/Widgets/BarPlot.hpp"
#include "ImFrame/Widgets/ScatterPlot.hpp"
#include "ImFrame/Widgets/HeatMap.hpp"

// ─── Layout ───────────────────────────────────────────────────────────────────
#include "ImFrame/Layout/ChildScope.hpp"
#include "ImFrame/Layout/Grid.hpp"

// ─── Animation ────────────────────────────────────────────────────────────────
#include "ImFrame/Anim/Tween.hpp"
#include "ImFrame/Anim/AnimatedValue.hpp"
#include "ImFrame/Anim/Easing.hpp"
#include "ImFrame/Anim/Sequence.hpp"

// ─── Overlays ─────────────────────────────────────────────────────────────────
#include "ImFrame/Overlay/Toast.hpp"
#include "ImFrame/Overlay/Modal.hpp"
#include "ImFrame/Overlay/ContextMenu.hpp"

// ─── Developer Tooling (Debug builds only) ────────────────────────────────────
#if defined(IMF_DEV_TOOLS)
#include "ImFrame/DevTools/LogViewer.hpp"
#include "ImFrame/DevTools/PerfOverlay.hpp"
#include "ImFrame/DevTools/ThemeHotReload.hpp"
#endif

// ─── Rendering (Phases 24–26) ─────────────────────────────────────────────────
#include "ImFrame/Rendering/RenderContext.hpp"
#include "ImFrame/Rendering/Viewport.hpp"
#include "ImFrame/Rendering/Transform2D.hpp"
#include "ImFrame/Rendering/Camera2D.hpp"
#include "ImFrame/Rendering/DrawContext.hpp"
#include "ImFrame/Rendering/Canvas2D.hpp"
#include "ImFrame/Rendering/Ray3D.hpp"
#include "ImFrame/Rendering/Transform3D.hpp"
#include "ImFrame/Rendering/Camera3D.hpp"
#include "ImFrame/Rendering/Viewport3D.hpp"
#include "ImFrame/Rendering/Gizmo.hpp"

// ─── Tree (Phases 27–28) ──────────────────────────────────────────────────────
#include "ImFrame/Tree/Key.hpp"
#include "ImFrame/Tree/Component.hpp"
#include "ImFrame/Tree/Context.hpp"
#include "ImFrame/Tree/Element.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Tree/Primitives/Box.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/Stack.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/SizedBox.hpp"
#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "ImFrame/Tree/Primitives/Spacer.hpp"
#include "ImFrame/Tree/State.hpp"
#include "ImFrame/Tree/Signal.hpp"
#include "ImFrame/Tree/Computed.hpp"
#include "ImFrame/Tree/InheritedWidget.hpp"
#include "ImFrame/Tree/Portal.hpp"
#include "ImFrame/Tree/VirtualList.hpp"

/**
 * @namespace ImFrame
 * @brief     Root namespace for the ImFrame immediate-mode UI framework.
 */
namespace ImFrame {
} // namespace ImFrame
