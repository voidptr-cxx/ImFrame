/**
 * @file     EmscriptenWebGPUBackend.cpp
 * @brief    EmscriptenWebGPUBackend — Init, Poll, BeginFrame, EndFrame, Shutdown
 *
 * @internal
 * UNVERIFIED — see EmscriptenWebGPUBackend.hpp.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "EmscriptenWebGPUBackend.hpp"
#include "Platform/InputTranslationEmscripten.hpp"
#include "Platform/SurfaceEmscripten.hpp"

#include "ImFrame/Utility/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_wgpu.h>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace ImFrame::Internal {

namespace {

constexpr double  CLEAR_R = 0.06;
constexpr double  CLEAR_G = 0.06;
constexpr double  CLEAR_B = 0.06;
constexpr int      NUM_FRAMES_IN_FLIGHT = 3;
constexpr WGPUTextureFormat CANVAS_FORMAT_FALLBACK = WGPUTextureFormat_BGRA8Unorm;

namespace IT = InputTranslationEmscripten;

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

EmscriptenWebGPUBackend::~EmscriptenWebGPUBackend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult EmscriptenWebGPUBackend::Init(const WindowConfig& config)
{
    if (_initialised) return std::unexpected(Error::AlreadyInitialised);
    if (config.Width <= 0 || config.Height <= 0) return std::unexpected(Error::InvalidArgument);

    _config         = config;
    _canvasSelector = std::string(config.EmscriptenCanvasSelector);

    WebGPUDeviceSetupResult setup{};
    if (auto r = SetUpWebGPUDevice(setup); !r) return r;
    _instance              = setup.instance;
    _adapter               = setup.adapter;
    _device                = setup.device;
    _queue                 = setup.queue;
    _maxTextureDimension2D = setup.maxTextureDimension2D;

    if (auto r = CreateSurface(); !r) { Shutdown(); return r; }
    if (auto r = InitImGui(); !r) { Shutdown(); return r; }

    RegisterDomCallbacks();

    _lastFrameTime = emscripten_get_now();
    _initialised   = true;
    return {};
}

// ─── CreateSurface ────────────────────────────────────────────────────────────

VoidResult EmscriptenWebGPUBackend::CreateSurface()
{
    WGPUSurface surface = CreateEmscriptenSurface(_instance, _canvasSelector);
    if (!surface) return std::unexpected(Error::GraphicsInitFailed);

    double cssWidth  = static_cast<double>(_config.Width);
    double cssHeight = static_cast<double>(_config.Height);
    emscripten_get_element_css_size(_canvasSelector.c_str(), &cssWidth, &cssHeight);
    double dpr = emscripten_get_device_pixel_ratio();

    _lastCssWidth  = static_cast<int>(cssWidth);
    _lastCssHeight = static_cast<int>(cssHeight);

    auto pixelWidth  = static_cast<std::uint32_t>(cssWidth * dpr);
    auto pixelHeight = static_cast<std::uint32_t>(cssHeight * dpr);

    return _surface.Configure(surface, _instance, _adapter, _device, pixelWidth, pixelHeight, _config.VSync);
}

// ─── InitImGui ────────────────────────────────────────────────────────────────

VoidResult EmscriptenWebGPUBackend::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (_config.Docking) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Multi-viewport is not supported on Emscripten — single canvas only.

    ImGui::StyleColorsDark();

    WGPUTextureFormat fmt = _surface.IsValid() ? _surface.Format() : CANVAS_FORMAT_FALLBACK;

    ImGui_ImplWGPU_InitInfo initInfo{};
    initInfo.Device             = _device;
    initInfo.NumFramesInFlight  = NUM_FRAMES_IN_FLIGHT;
    initInfo.RenderTargetFormat = fmt;
    initInfo.DepthStencilFormat = WGPUTextureFormat_Undefined;

    if (!ImGui_ImplWGPU_Init(&initInfo)) {
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }
    return {};
}

// ─── RegisterDomCallbacks ─────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::RegisterDomCallbacks()
{
    const char* target = _canvasSelector.c_str();
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, &OnKeyDown);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, &OnKeyUp);
    emscripten_set_mousemove_callback(target, this, true, &OnMouseMove);
    emscripten_set_mousedown_callback(target, this, true, &OnMouseDown);
    emscripten_set_mouseup_callback(target, this, true, &OnMouseUp);
    emscripten_set_wheel_callback(target, this, true, &OnWheel);
    emscripten_set_touchstart_callback(target, this, true, &OnTouchStart);
    emscripten_set_touchmove_callback(target, this, true, &OnTouchMove);
    emscripten_set_touchend_callback(target, this, true, &OnTouchEnd);
}

void EmscriptenWebGPUBackend::UnregisterDomCallbacks()
{
    const char* target = _canvasSelector.c_str();
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_mousemove_callback(target, nullptr, true, nullptr);
    emscripten_set_mousedown_callback(target, nullptr, true, nullptr);
    emscripten_set_mouseup_callback(target, nullptr, true, nullptr);
    emscripten_set_wheel_callback(target, nullptr, true, nullptr);
    emscripten_set_touchstart_callback(target, nullptr, true, nullptr);
    emscripten_set_touchmove_callback(target, nullptr, true, nullptr);
    emscripten_set_touchend_callback(target, nullptr, true, nullptr);
}

// ─── DOM callbacks ────────────────────────────────────────────────────────────
// No ImGui_ImplSDL3-equivalent platform backend exists for Emscripten — these
// feed ImGui's event-based IO API directly, the same way every existing
// ImGui_Impl*_ProcessEvent() does internally. They also push the translated
// InputEvent into the queue DrainInputEvents() returns to the App layer.

bool EmscriptenWebGPUBackend::OnKeyDown(int /*eventType*/, const EmscriptenKeyboardEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    self->_inputQueue.push_back(IT::TranslateKey(*e, KeyAction::Pressed));
    if (e->charValue[0] != '\0') {
        ImGui::GetIO().AddInputCharactersUTF8(e->charValue);
    }
    return true; // Consume — prevents default browser key handling (scrolling, etc.).
}

bool EmscriptenWebGPUBackend::OnKeyUp(int /*eventType*/, const EmscriptenKeyboardEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    self->_inputQueue.push_back(IT::TranslateKey(*e, KeyAction::Released));
    return true;
}

bool EmscriptenWebGPUBackend::OnMouseMove(int /*eventType*/, const EmscriptenMouseEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    self->_inputQueue.push_back(IT::TranslateMouseMove(*e));
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(e->targetX), static_cast<float>(e->targetY));
    return true;
}

bool EmscriptenWebGPUBackend::OnMouseDown(int /*eventType*/, const EmscriptenMouseEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    self->_inputQueue.push_back(IT::TranslateMouseButton(*e, KeyAction::Pressed));
    return true;
}

bool EmscriptenWebGPUBackend::OnMouseUp(int /*eventType*/, const EmscriptenMouseEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    self->_inputQueue.push_back(IT::TranslateMouseButton(*e, KeyAction::Released));
    return true;
}

bool EmscriptenWebGPUBackend::OnWheel(int /*eventType*/, const EmscriptenWheelEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    self->_inputQueue.push_back(IT::TranslateMouseWheel(*e));
    return true;
}

bool EmscriptenWebGPUBackend::OnTouchStart(int /*eventType*/, const EmscriptenTouchEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    for (int i = 0; i < e->numTouches; ++i) {
        if (!e->touches[i].isChanged) continue;
        self->_inputQueue.push_back(IT::TranslateTouchPoint(e->touches[i], TouchPhase::Began,
                                                              self->_lastCssWidth, self->_lastCssHeight));
    }
    return true;
}

bool EmscriptenWebGPUBackend::OnTouchMove(int /*eventType*/, const EmscriptenTouchEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    for (int i = 0; i < e->numTouches; ++i) {
        if (!e->touches[i].isChanged) continue;
        self->_inputQueue.push_back(IT::TranslateTouchPoint(e->touches[i], TouchPhase::Moved,
                                                              self->_lastCssWidth, self->_lastCssHeight));
    }
    return true;
}

bool EmscriptenWebGPUBackend::OnTouchEnd(int /*eventType*/, const EmscriptenTouchEvent* e, void* userData)
{
    auto* self = static_cast<EmscriptenWebGPUBackend*>(userData);
    for (int i = 0; i < e->numTouches; ++i) {
        if (!e->touches[i].isChanged) continue;
        self->_inputQueue.push_back(IT::TranslateTouchPoint(e->touches[i], TouchPhase::Ended,
                                                              self->_lastCssWidth, self->_lastCssHeight));
    }
    return true;
}

// ─── PollCanvasResize ─────────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::PollCanvasResize()
{
    double cssWidth = 0.0;
    double cssHeight = 0.0;
    emscripten_get_element_css_size(_canvasSelector.c_str(), &cssWidth, &cssHeight);

    int w = static_cast<int>(cssWidth);
    int h = static_cast<int>(cssHeight);
    if (w == _lastCssWidth && h == _lastCssHeight) return;

    _lastCssWidth  = w;
    _lastCssHeight = h;
    _inputQueue.push_back(WindowResizeEvent{ PrimaryWindow, w, h });

    double dpr = emscripten_get_device_pixel_ratio();
    if (auto r = _surface.Resize(static_cast<std::uint32_t>(cssWidth * dpr),
                                  static_cast<std::uint32_t>(cssHeight * dpr)); !r) {
        IMF_WARN("WebGPU (Emscripten) surface resize to {}x{} failed", w, h);
    }
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo EmscriptenWebGPUBackend::Poll()
{
    IMF_ASSERT(_initialised);

    wgpuInstanceProcessEvents(_instance);
    PollCanvasResize();

    double now = emscripten_get_now();
    float dt = static_cast<float>((now - _lastFrameTime) / 1000.0); // ms -> seconds.
    _lastFrameTime = now;

    return FrameInfo{
        .ShouldClose            = _shouldClose,
        .DeltaTime              = dt > 0.0f ? dt : (1.0f / 60.0f),
        .DisplayRefreshInterval = 1.0f / 60.0f, // No equivalent of SDL_GetCurrentDisplayMode() queried here.
        .ActiveWindows          = { PrimaryWindow },
    };
}

// ─── BeginFrame ───────────────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::BeginFrame(WindowHandle /*handle*/)
{
    IMF_ASSERT(_initialised);

    WebGPUAcquireResult acquired = _surface.AcquireCurrentTexture();
    if (acquired.fatal || !acquired.ok) {
        _frameStarted = false;
        ImGui_ImplWGPU_NewFrame();
        ImGui::NewFrame();
        return;
    }
    if (acquired.needsReconfigure) {
        // Force a reconfigure on the next PollCanvasResize() pass.
        _lastCssWidth = -1;
    }

    _currentTexture = acquired.texture;

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format        = _surface.Format();
    viewDesc.dimension      = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect          = WGPUTextureAspect_All;
    _currentView = wgpuTextureCreateView(_currentTexture, &viewDesc);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(_lastCssWidth), static_cast<float>(_lastCssHeight));

    _frameStarted = true;
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::EndFrame(WindowHandle /*handle*/)
{
    IMF_ASSERT(_initialised);

    ImGui::Render();

    if (!_frameStarted) {
        ImGui::EndFrame();
        return;
    }

    WGPUCommandEncoderDescriptor encDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view      = _currentView;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp      = WGPULoadOp_Clear;
    colorAttachment.storeOp      = WGPUStoreOp_Store;
    colorAttachment.clearValue   = { CLEAR_R, CLEAR_G, CLEAR_B, 1.0 };

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments      = &colorAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmdBufDesc{};
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);
    wgpuQueueSubmit(_queue, 1, &cmdBuffer);
    wgpuCommandBufferRelease(cmdBuffer);
    wgpuCommandEncoderRelease(encoder);

    _surface.Present();

    ReleaseFrameTexture();
    _frameStarted = false;
}

// ─── ReleaseFrameTexture ──────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::ReleaseFrameTexture()
{
    if (_currentView) {
        wgpuTextureViewRelease(_currentView);
        _currentView = nullptr;
    }
    if (_currentTexture) {
        wgpuTextureRelease(_currentTexture);
        _currentTexture = nullptr;
    }
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::Shutdown()
{
    if (!_initialised) return;

    UnregisterDomCallbacks();

    if (_device) {
        ImGui_ImplWGPU_Shutdown();
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    _surface.Destroy();

    if (_queue)    { wgpuQueueRelease(_queue);       _queue    = nullptr; }
    if (_device)   { wgpuDeviceRelease(_device);     _device   = nullptr; }
    if (_adapter)  { wgpuAdapterRelease(_adapter);   _adapter  = nullptr; }
    if (_instance) { wgpuInstanceRelease(_instance); _instance = nullptr; }

    _initialised   = false;
    _frameStarted  = false;
    _shouldClose   = false;
    _inputQueue.clear();
    _drainBuffer.clear();
}

// ─── NativeHandle ─────────────────────────────────────────────────────────────

void* EmscriptenWebGPUBackend::NativeHandle() const
{
    return nullptr; // No platform window object exists on Emscripten.
}

// ─── CancelClose ──────────────────────────────────────────────────────────────

void EmscriptenWebGPUBackend::CancelClose() noexcept
{
    _shouldClose = false;
}

// ─── WindowDpiScale ───────────────────────────────────────────────────────────

float EmscriptenWebGPUBackend::WindowDpiScale(WindowHandle /*handle*/) const
{
    return static_cast<float>(emscripten_get_device_pixel_ratio());
}

// ─── WindowSize ───────────────────────────────────────────────────────────────

WindowExtent EmscriptenWebGPUBackend::WindowSize(WindowHandle /*handle*/) const
{
    double dpr = emscripten_get_device_pixel_ratio();
    return WindowExtent{
        static_cast<int>(_lastCssWidth * dpr),
        static_cast<int>(_lastCssHeight * dpr),
    };
}

// ─── WindowIsMinimized ────────────────────────────────────────────────────────

bool EmscriptenWebGPUBackend::WindowIsMinimized(WindowHandle /*handle*/) const
{
    EmscriptenVisibilityChangeEvent vis{};
    if (emscripten_get_visibility_status(&vis) == EMSCRIPTEN_RESULT_SUCCESS) {
        return vis.hidden;
    }
    return false;
}

// ─── WindowIsFocused ──────────────────────────────────────────────────────────

bool EmscriptenWebGPUBackend::WindowIsFocused(WindowHandle /*handle*/) const
{
    return !WindowIsMinimized(PrimaryWindow);
}

// ─── CreateWindow / DestroyWindow ─────────────────────────────────────────────

WindowHandle EmscriptenWebGPUBackend::CreateWindow(const WindowConfig& /*config*/)
{
    return PrimaryWindow; // Multi-canvas is out of scope this phase — see proposal.
}

void EmscriptenWebGPUBackend::DestroyWindow(WindowHandle /*handle*/)
{
    // No secondary window exists to destroy.
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> EmscriptenWebGPUBackend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext EmscriptenWebGPUBackend::GetNativeGraphicsContext() const
{
    return WebGPUContext{
        .Device               = static_cast<void*>(_device),
        .Queue                = static_cast<void*>(_queue),
        .PreferredFormat       = static_cast<std::uint32_t>(_surface.IsValid() ? _surface.Format() : 0),
        .MaxTextureDimension2D = _maxTextureDimension2D,
        .IsEmscripten          = true,
    };
}

} // namespace ImFrame::Internal
