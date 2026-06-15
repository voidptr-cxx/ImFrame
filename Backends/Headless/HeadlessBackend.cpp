/**
 * @file     HeadlessBackend.cpp
 * @brief    Offscreen IBackend implementation for CI tests and server-side rendering
 *
 * @internal
 * No platform windowing or GPU library is used. The ImGui context is driven
 * purely by direct API calls. The pixel buffer is CPU-resident and zero-filled.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "HeadlessBackend.hpp"

#include <imgui.h>

namespace ImFrame::Internal {

// ─── Destructor ───────────────────────────────────────────────────────────────

HeadlessBackend::~HeadlessBackend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult HeadlessBackend::Init(const WindowConfig& config)
{
    if (_initialised) {
        return std::unexpected(Error::AlreadyInitialised);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io       = ImGui::GetIO();
    io.ConfigFlags   |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags   |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize    = ImVec2(static_cast<float>(config.Width),
                               static_cast<float>(config.Height));
    io.DeltaTime      = 1.0f / 60.0f;
    io.IniFilename    = nullptr; // No file-backed settings for headless context.

    // Build a minimal font atlas so ImGui::NewFrame() does not assert.
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int            w      = 0;
    int            h      = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));

    _width  = config.Width;
    _height = config.Height;

    // Allocate a zero-filled RGBA8 framebuffer (clear colour = transparent black).
    _pixelBuffer.assign(static_cast<std::size_t>(_width) * _height * 4, std::byte{0});

    _initialised = true;
    return {};
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo HeadlessBackend::Poll()
{
    IMF_ASSERT(_initialised);
    FrameInfo info;
    info.ShouldClose            = false;
    info.DeltaTime              = 1.0f / 60.0f;
    info.DisplayRefreshInterval = 1.0f / 60.0f;
    info.ActiveWindows          = { PrimaryWindow };
    return info;
}

// ─── BeginFrame ───────────────────────────────────────────────────────────────

void HeadlessBackend::BeginFrame(WindowHandle /*handle*/)
{
    IMF_ASSERT(_initialised);
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void HeadlessBackend::EndFrame(WindowHandle /*handle*/)
{
    IMF_ASSERT(_initialised);
    ImGui::Render();
    // Draw data is discarded — no GPU submission in headless mode.
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void HeadlessBackend::Shutdown()
{
    if (!_initialised) {
        return;
    }
    ImGui::DestroyContext();
    _pixelBuffer.clear();
    _inputQueue.clear();
    _drainBuffer.clear();
    _initialised = false;
}

// ─── WindowSize ───────────────────────────────────────────────────────────────

WindowExtent HeadlessBackend::WindowSize(WindowHandle /*handle*/) const
{
    return WindowExtent{ _width, _height };
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> HeadlessBackend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext HeadlessBackend::GetNativeGraphicsContext() const
{
    return HeadlessContext{ _width, _height, HeadlessPixelFormat::RGBA8 };
}

// ─── InjectInputEvent ─────────────────────────────────────────────────────────

void HeadlessBackend::InjectInputEvent(InputEvent event)
{
    _inputQueue.push_back(std::move(event));
}

// ─── ReadPixels ───────────────────────────────────────────────────────────────

std::vector<std::byte> HeadlessBackend::ReadPixels() const
{
    return _pixelBuffer;
}

// ─── CreateWindow ─────────────────────────────────────────────────────────────

WindowHandle HeadlessBackend::CreateWindow(const WindowConfig& /*config*/)
{
    return ++_nextHandle;
}

// ─── DestroyWindow ────────────────────────────────────────────────────────────

void HeadlessBackend::DestroyWindow(WindowHandle /*handle*/)
{
    // Virtual windows have no OS resource to release.
}

} // namespace ImFrame::Internal
