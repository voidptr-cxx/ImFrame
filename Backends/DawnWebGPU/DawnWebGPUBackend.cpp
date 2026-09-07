/**
 * @file     DawnWebGPUBackend.cpp
 * @brief    SDL3 + Dawn WebGPU backend — Init, Poll, BeginFrame, EndFrame, Shutdown
 *
 * @internal
 * Initialisation sequence:
 *   InitSDL -> create SDL window (hidden) -> SetUpWebGPUDevice (WebGPUDeviceSetup.cpp,
 *   shared with the Emscripten path) -> CreateSurfaceFor(primary) -> InitImGui ->
 *   show window (windowed mode only).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "DawnWebGPUBackend.hpp"
#include "../SDL3Vulkan/InputTranslation.hpp"
#include "NativeSurface.hpp"
#include "ViewportWebGPU.hpp"

#include "ImFrame/Utility/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>

#include <SDL3/SDL_properties.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace ImFrame::Internal {

namespace {

constexpr float DELTA_TIME_MAX     = 0.1f;
constexpr float DEFAULT_DELTA_TIME = 1.0f / 60.0f;
constexpr double CLEAR_R           = 0.06;
constexpr double CLEAR_G           = 0.06;
constexpr double CLEAR_B           = 0.06;

constexpr int NUM_FRAMES_IN_FLIGHT = 3; // ImGui_ImplWGPU_InitInfo::NumFramesInFlight default.

// WebGPU's spec-mandated row alignment for buffer<->texture copies. No named
// constant exists in webgpu.h (unlike D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) —
// 256 is fixed by the WebGPU specification itself.
constexpr std::uint64_t COPY_BYTES_PER_ROW_ALIGNMENT = 256;

float ClampDelta(float dt) noexcept
{
    if (dt < 0.0f)           return 0.0f;
    if (dt > DELTA_TIME_MAX) return DELTA_TIME_MAX;
    return dt;
}

constexpr std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

DawnWebGPUBackend::~DawnWebGPUBackend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult DawnWebGPUBackend::Init(const WindowConfig& config)
{
    if (_initialised) return std::unexpected(Error::AlreadyInitialised);
    if (config.Width <= 0 || config.Height <= 0) return std::unexpected(Error::InvalidArgument);

    _primary.config = config;

    if (auto r = InitSDL(config); !r) return r;

    SDL_WindowFlags wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    _primary.sdlWindow = SDL_CreateWindow(std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!_primary.sdlWindow) {
        Shutdown();
        return std::unexpected(Error::WindowCreationFailed);
    }

    WebGPUDeviceSetupResult setup{};
    if (auto r = SetUpWebGPUDevice(setup); !r) { Shutdown(); return r; }
    _instance              = setup.instance;
    _adapter               = setup.adapter;
    _device                = setup.device;
    _queue                 = setup.queue;
    _maxTextureDimension2D = setup.maxTextureDimension2D;

    if (auto r = CreateSurfaceFor(_primary, _primaryIsHeadless); !r) { Shutdown(); return r; }
    if (auto r = InitImGui(); !r) { Shutdown(); return r; }

    if (!_primaryIsHeadless) SDL_ShowWindow(_primary.sdlWindow);

    SDL_WindowID primaryId = SDL_GetWindowID(_primary.sdlWindow);
    _sdlIdToHandle[primaryId] = PrimaryWindow;

    _lastPerfCount = SDL_GetPerformanceCounter();
    _initialised   = true;
    return {};
}

// ─── InitSDL ──────────────────────────────────────────────────────────────────

VoidResult DawnWebGPUBackend::InitSDL(const WindowConfig& /*config*/)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return std::unexpected(Error::WindowCreationFailed);
    }
    _sdlInitialised = true;
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    return {};
}

// ─── CreateSurfaceFor ─────────────────────────────────────────────────────────

VoidResult DawnWebGPUBackend::CreateSurfaceFor(WindowData& wd, bool headless)
{
    wd.headless = headless;

    if (headless) {
        WGPUTextureDescriptor desc{};
        desc.usage         = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        desc.dimension      = WGPUTextureDimension_2D;
        desc.size            = { static_cast<std::uint32_t>(wd.config.Width),
                                  static_cast<std::uint32_t>(wd.config.Height), 1 };
        desc.format          = _headlessFormat;
        desc.mipLevelCount    = 1;
        desc.sampleCount      = 1;

        wd.headlessTarget = wgpuDeviceCreateTexture(_device, &desc);
        if (!wd.headlessTarget) return std::unexpected(Error::GraphicsInitFailed);
        return {};
    }

    WGPUSurface surface = CreateNativeSurface(_instance, wd.sdlWindow);
    if (!surface) return std::unexpected(Error::GraphicsInitFailed);

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);

    return wd.surface.Configure(surface, _instance, _adapter, _device, static_cast<std::uint32_t>(w),
                                 static_cast<std::uint32_t>(h), wd.config.VSync);
}

// ─── InitImGui ────────────────────────────────────────────────────────────────

VoidResult DawnWebGPUBackend::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (_primary.config.Docking)   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (_primary.config.Viewports) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForOther(_primary.sdlWindow)) {
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    ImGui_ImplWGPU_InitInfo initInfo{};
    initInfo.Device            = _device;
    initInfo.NumFramesInFlight = NUM_FRAMES_IN_FLIGHT;
    initInfo.RenderTargetFormat = _primary.headless ? _headlessFormat : _primary.surface.Format();
    initInfo.DepthStencilFormat = WGPUTextureFormat_Undefined;

    if (!ImGui_ImplWGPU_Init(&initInfo)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }
    return {};
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo DawnWebGPUBackend::Poll()
{
    IMF_ASSERT(_initialised);

    namespace IT = InputTranslation;

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
            case SDL_EVENT_QUIT:
                _shouldClose = true;
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                _inputQueue.push_back(IT::TranslateKey(event.key));
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (auto opt = IT::TranslateTextInput(event.text)) {
                    _inputQueue.push_back(*opt);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                _inputQueue.push_back(IT::TranslateMouseButton(event.button));
                break;

            case SDL_EVENT_MOUSE_MOTION:
                _inputQueue.push_back(IT::TranslateMouseMotion(event.motion));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                _inputQueue.push_back(IT::TranslateMouseWheel(event.wheel));
                break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                _inputQueue.push_back(IT::TranslateGamepadAxis(event.gaxis));
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                _inputQueue.push_back(IT::TranslateGamepadButton(event.gbutton));
                break;

            case SDL_EVENT_FINGER_DOWN:
                _inputQueue.push_back(IT::TranslateTouch(event.tfinger, TouchPhase::Began));
                break;
            case SDL_EVENT_FINGER_MOTION:
                _inputQueue.push_back(IT::TranslateTouch(event.tfinger, TouchPhase::Moved));
                break;
            case SDL_EVENT_FINGER_UP:
                _inputQueue.push_back(IT::TranslateTouch(event.tfinger, TouchPhase::Ended));
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowResize(event.window, wh));
                if (wh == PrimaryWindow) {
                    _primary.needsResize = true;
                } else if (auto* sec = FindWindow(wh)) {
                    sec->needsResize = true;
                }
                break;
            }

            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowFocus(wh, true));
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowFocus(wh, false));
                break;
            }

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                WindowHandle wh = HandleForSDLWindow(event.window.windowID);
                _inputQueue.push_back(IT::TranslateWindowClose(wh));
                if (wh == PrimaryWindow) _shouldClose = true;
                break;
            }

            default:
                break;
        }
    }

    // Dawn requires this to be pumped regularly to drain async callbacks
    // (buffer mapping, error callbacks) on the render thread.
    wgpuInstanceProcessEvents(_instance);

    uint64_t now  = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    float dt = (_lastPerfCount > 0 && freq > 0)
                   ? ClampDelta(static_cast<float>(now - _lastPerfCount) / static_cast<float>(freq))
                   : DEFAULT_DELTA_TIME;
    _lastPerfCount = now;

    float refreshInterval = DEFAULT_DELTA_TIME;
    SDL_DisplayID displayId = SDL_GetDisplayForWindow(_primary.sdlWindow);
    if (displayId != 0) {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
        if (mode && mode->refresh_rate > 0.0f) {
            refreshInterval = 1.0f / mode->refresh_rate;
        }
    }

    std::vector<WindowHandle> active;
    active.reserve(1 + _secondaryWindows.size());
    active.push_back(PrimaryWindow);
    for (const auto& [h, _] : _secondaryWindows) active.push_back(h);

    return FrameInfo{
        .ShouldClose            = _shouldClose,
        .DeltaTime              = dt,
        .DisplayRefreshInterval = refreshInterval,
        .ActiveWindows          = std::move(active),
    };
}

// ─── BeginFrame ───────────────────────────────────────────────────────────────

void DawnWebGPUBackend::BeginFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    if (wd.needsResize) {
        HandleResize(wd);
        wd.needsResize = false;
    }

    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &pw, &ph);
    if (!wd.headless && (pw == 0 || ph == 0)) {
        wd.frameStarted = false;
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return;
    }

    if (wd.headless) {
        wd.currentTexture = wd.headlessTarget; // Non-owning — persistent texture, never released per-frame.
    } else {
        WebGPUAcquireResult acquired = wd.surface.AcquireCurrentTexture();
        if (acquired.fatal) {
            IMF_FATAL("[WebGPU] surface lost — device recreation not implemented this phase");
            wd.frameStarted = false;
            ImGui_ImplWGPU_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            return;
        }
        if (!acquired.ok) {
            // Timeout/Outdated — no usable texture this frame; reconfigure happens via needsResize next Poll().
            wd.frameStarted = false;
            ImGui_ImplWGPU_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            return;
        }
        wd.currentTexture = acquired.texture; // Owning — released in EndFrame/ReleaseFrameTexture.
        if (acquired.needsReconfigure) wd.needsResize = true;
    }

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = wd.headless ? _headlessFormat : wd.surface.Format();
    viewDesc.dimension        = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount     = 1;
    viewDesc.arrayLayerCount   = 1;
    viewDesc.aspect            = WGPUTextureAspect_All;
    wd.currentView = wgpuTextureCreateView(wd.currentTexture, &viewDesc);

    wd.frameStarted = true;

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void DawnWebGPUBackend::EndFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    ImGui::Render();

    if (!wd.frameStarted) {
        ImGui::EndFrame();
        return;
    }

    WGPUCommandEncoderDescriptor encDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view       = wd.currentView;
    colorAttachment.depthSlice  = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp       = WGPULoadOp_Clear;
    colorAttachment.storeOp       = WGPUStoreOp_Store;
    colorAttachment.clearValue    = { CLEAR_R, CLEAR_G, CLEAR_B, 1.0 };

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments      = &colorAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    // ReadPixels() support — headless only, mirroring Phase 20-22's scoping.
    bool capturedForReadback = false;
    if (handle == PrimaryWindow && wd.headless) {
        std::uint64_t rowPitch   = AlignUp(static_cast<std::uint64_t>(wd.config.Width) * 4, COPY_BYTES_PER_ROW_ALIGNMENT);
        std::uint64_t bufferSize = rowPitch * static_cast<std::uint64_t>(wd.config.Height);

        if (!_readbackBuffer || _readbackBufferSize < bufferSize) {
            if (_readbackBuffer) wgpuBufferRelease(_readbackBuffer);
            WGPUBufferDescriptor bufDesc{};
            bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
            bufDesc.size   = bufferSize;
            _readbackBuffer     = wgpuDeviceCreateBuffer(_device, &bufDesc);
            _readbackBufferSize = bufferSize;
        }

        if (_readbackBuffer) {
            WGPUTexelCopyTextureInfo src{};
            src.texture = wd.currentTexture;

            WGPUTexelCopyBufferInfo dst{};
            dst.buffer              = _readbackBuffer;
            dst.layout.bytesPerRow   = static_cast<std::uint32_t>(rowPitch);
            dst.layout.rowsPerImage  = static_cast<std::uint32_t>(wd.config.Height);

            WGPUExtent3D copySize{ static_cast<std::uint32_t>(wd.config.Width),
                                    static_cast<std::uint32_t>(wd.config.Height), 1 };
            wgpuCommandEncoderCopyTextureToBuffer(encoder, &src, &dst, &copySize);

            _readbackRowPitch = rowPitch;
            _readbackWidth    = static_cast<std::uint32_t>(wd.config.Width);
            _readbackHeight   = static_cast<std::uint32_t>(wd.config.Height);
            capturedForReadback = true;
        }
    }

    WGPUCommandBufferDescriptor cmdBufDesc{};
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);
    wgpuQueueSubmit(_queue, 1, &cmdBuffer);
    wgpuCommandBufferRelease(cmdBuffer);
    wgpuCommandEncoderRelease(encoder);

    if (!wd.headless) {
        wd.surface.Present();
    }

    if (handle == PrimaryWindow && capturedForReadback) {
        // Block until this frame's copy lands before the buffer is mapped in
        // ReadPixels() — debug/test utility, not a hot-path call.
        struct DoneFlag { bool done = false; };
        DoneFlag done{};
        WGPUQueueWorkDoneCallbackInfo cbInfo{};
        cbInfo.mode      = WGPUCallbackMode_WaitAnyOnly;
        cbInfo.callback  = [](WGPUQueueWorkDoneStatus, WGPUStringView, void* userdata1, void*) {
            static_cast<DoneFlag*>(userdata1)->done = true;
        };
        cbInfo.userdata1 = &done;

        WGPUFuture future = wgpuQueueOnSubmittedWorkDone(_queue, cbInfo);
        WGPUFutureWaitInfo waitInfo{};
        waitInfo.future = future;
        wgpuInstanceWaitAny(_instance, 1, &waitInfo, UINT64_MAX);
        _hasReadback = done.done;
    }

    ReleaseFrameTexture(wd);
    wd.frameStarted = false;
}

// ─── ReleaseFrameTexture ──────────────────────────────────────────────────────

void DawnWebGPUBackend::ReleaseFrameTexture(WindowData& wd)
{
    if (wd.currentView) {
        wgpuTextureViewRelease(wd.currentView);
        wd.currentView = nullptr;
    }
    if (wd.currentTexture && !wd.headless) {
        wgpuTextureRelease(wd.currentTexture); // Owning ref returned by AcquireCurrentTexture().
    }
    wd.currentTexture = nullptr; // Headless: non-owning pointer to the persistent target — just clear it.
}

// ─── HandleResize ─────────────────────────────────────────────────────────────

void DawnWebGPUBackend::HandleResize(WindowData& wd)
{
    if (wd.headless) return; // Offscreen target size is fixed at creation.

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);
    if (w == 0 || h == 0) return; // Still minimized.

    if (auto r = wd.surface.Resize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)); !r) {
        IMF_WARN("WebGPU surface resize to {}x{} failed", w, h);
    }
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void DawnWebGPUBackend::Shutdown()
{
    if (!_sdlInitialised) return;

    if (_device) {
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    for (auto& [_, wd] : _secondaryWindows) {
        wd.surface.Destroy();
        if (wd.sdlWindow) SDL_DestroyWindow(wd.sdlWindow);
    }
    _secondaryWindows.clear();
    _sdlIdToHandle.clear();

    _primary.surface.Destroy();
    if (_primary.headlessTarget) {
        wgpuTextureRelease(_primary.headlessTarget);
        _primary.headlessTarget = nullptr;
    }
    if (_primary.sdlWindow) {
        SDL_DestroyWindow(_primary.sdlWindow);
        _primary.sdlWindow = nullptr;
    }

    if (_readbackBuffer) {
        wgpuBufferRelease(_readbackBuffer);
        _readbackBuffer = nullptr;
    }
    _readbackBufferSize = 0;

    if (_queue)    { wgpuQueueRelease(_queue);    _queue    = nullptr; }
    if (_device)   { wgpuDeviceRelease(_device);  _device   = nullptr; }
    if (_adapter)  { wgpuAdapterRelease(_adapter); _adapter = nullptr; }
    if (_instance) { wgpuInstanceRelease(_instance); _instance = nullptr; }

    SDL_Quit();

    _initialised    = false;
    _sdlInitialised = false;
    _shouldClose    = false;
    _lastPerfCount  = 0;
    _hasReadback    = false;
    _inputQueue.clear();
    _drainBuffer.clear();
}

// ─── NativeHandle ─────────────────────────────────────────────────────────────

void* DawnWebGPUBackend::NativeHandle() const
{
    return _primary.sdlWindow;
}

// ─── CancelClose ──────────────────────────────────────────────────────────────

void DawnWebGPUBackend::CancelClose() noexcept
{
    _shouldClose = false;
}

// ─── WindowDpiScale ───────────────────────────────────────────────────────────

float DawnWebGPUBackend::WindowDpiScale(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return 1.0f;
    return SDL_GetWindowDisplayScale(win);
}

// ─── WindowSize ───────────────────────────────────────────────────────────────

WindowExtent DawnWebGPUBackend::WindowSize(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return {};
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    return WindowExtent{ w, h };
}

// ─── WindowIsMinimized ────────────────────────────────────────────────────────

bool DawnWebGPUBackend::WindowIsMinimized(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return false;
    return (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED) != 0;
}

// ─── WindowIsFocused ──────────────────────────────────────────────────────────

bool DawnWebGPUBackend::WindowIsFocused(WindowHandle handle) const
{
    SDL_Window* win = nullptr;
    if (handle == PrimaryWindow) {
        win = _primary.sdlWindow;
    } else if (const auto* wd = FindWindow(handle)) {
        win = wd->sdlWindow;
    }
    if (!win) return false;
    return (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

// ─── CreateWindow ─────────────────────────────────────────────────────────────

WindowHandle DawnWebGPUBackend::CreateWindow(const WindowConfig& config)
{
    IMF_ASSERT(_initialised);

    WindowData wd{};
    wd.config = config;

    SDL_WindowFlags wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    wd.sdlWindow = SDL_CreateWindow(std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!wd.sdlWindow) return PrimaryWindow;

    if (!CreateSurfaceFor(wd, false)) {
        SDL_DestroyWindow(wd.sdlWindow);
        return PrimaryWindow;
    }

    WindowHandle wh    = _nextHandle++;
    SDL_WindowID sdlId = SDL_GetWindowID(wd.sdlWindow);
    _sdlIdToHandle[sdlId] = wh;

    _inputQueue.push_back(WindowResizeEvent{ wh, config.Width, config.Height });
    _secondaryWindows[wh] = std::move(wd);
    return wh;
}

// ─── DestroyWindow ────────────────────────────────────────────────────────────

void DawnWebGPUBackend::DestroyWindow(WindowHandle handle)
{
    if (handle == PrimaryWindow) return;

    auto it = _secondaryWindows.find(handle);
    if (it == _secondaryWindows.end()) return;

    WindowData& wd = it->second;
    wd.surface.Destroy();
    if (wd.sdlWindow) {
        SDL_WindowID sdlId = SDL_GetWindowID(wd.sdlWindow);
        _sdlIdToHandle.erase(sdlId);
        SDL_DestroyWindow(wd.sdlWindow);
    }

    _secondaryWindows.erase(it);
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> DawnWebGPUBackend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext DawnWebGPUBackend::GetNativeGraphicsContext() const
{
    return WebGPUContext{
        .Device               = static_cast<void*>(_device),
        .Queue                = static_cast<void*>(_queue),
        .PreferredFormat       = static_cast<std::uint32_t>(_primary.headless ? _headlessFormat : _primary.surface.Format()),
        .MaxTextureDimension2D = _maxTextureDimension2D,
        .IsEmscripten          = false,
    };
}

// ─── CreateViewportFramebuffer ────────────────────────────────────────────────

std::unique_ptr<IViewportFramebuffer> DawnWebGPUBackend::CreateViewportFramebuffer(
    std::uint32_t width, std::uint32_t height)
{
    if (!_initialised) return nullptr;
    return std::make_unique<ViewportFramebufferWebGPU>(
        _device,
        _queue,
        WGPUTextureFormat_BGRA8Unorm,
        width,
        height);
}

// ─── ReadPixels ───────────────────────────────────────────────────────────────

std::vector<std::byte> DawnWebGPUBackend::ReadPixels() const
{
    if (!_initialised || !_device) return {};
    if (!_primary.headless) return {};
    if (!_hasReadback || !_readbackBuffer || _readbackWidth == 0 || _readbackHeight == 0) return {};

    struct MapDoneFlag { bool done = false; bool ok = false; };
    MapDoneFlag mapDone{};
    WGPUBufferMapCallbackInfo mapCb{};
    mapCb.mode      = WGPUCallbackMode_WaitAnyOnly;
    mapCb.callback  = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        auto* flag = static_cast<MapDoneFlag*>(userdata1);
        flag->done = true;
        flag->ok   = (status == WGPUMapAsyncStatus_Success);
    };
    mapCb.userdata1 = &mapDone;

    WGPUFuture future = wgpuBufferMapAsync(_readbackBuffer, WGPUMapMode_Read, 0, _readbackBufferSize, mapCb);
    WGPUFutureWaitInfo waitInfo{};
    waitInfo.future = future;
    wgpuInstanceWaitAny(_instance, 1, &waitInfo, UINT64_MAX);

    if (!mapDone.ok) return {};

    const void* mapped = wgpuBufferGetConstMappedRange(_readbackBuffer, 0, _readbackBufferSize);
    if (!mapped) {
        wgpuBufferUnmap(_readbackBuffer);
        return {};
    }

    const std::size_t tightRowBytes = static_cast<std::size_t>(_readbackWidth) * 4;
    std::vector<std::byte> pixels(tightRowBytes * _readbackHeight);

    const auto* src = static_cast<const std::byte*>(mapped);
    for (std::uint32_t row = 0; row < _readbackHeight; ++row) {
        std::memcpy(pixels.data() + row * tightRowBytes, src + row * _readbackRowPitch, tightRowBytes);
    }

    wgpuBufferUnmap(_readbackBuffer);
    return pixels;
}

// ─── FindWindow ───────────────────────────────────────────────────────────────

DawnWebGPUBackend::WindowData* DawnWebGPUBackend::FindWindow(WindowHandle handle)
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

const DawnWebGPUBackend::WindowData* DawnWebGPUBackend::FindWindow(WindowHandle handle) const
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

WindowHandle DawnWebGPUBackend::HandleForSDLWindow(SDL_WindowID id) const
{
    auto it = _sdlIdToHandle.find(id);
    return (it != _sdlIdToHandle.end()) ? it->second : PrimaryWindow;
}

} // namespace ImFrame::Internal
