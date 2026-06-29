/**
 * @file     SDL3MetalBackend.mm
 * @brief    SDL3 + Metal backend — Init, Poll, BeginFrame, EndFrame, Shutdown
 *
 * @internal
 * Initialisation sequence:
 *   InitSDL → create SDL window (hidden) → CreateDevice → CreateMetalLayerFor(primary) →
 *   AllocateFrameResources(primary) → InitImGui → show window (windowed mode only).
 * Font atlas texture upload happens lazily on first RenderDrawData() via
 * imgui_impl_metal's internal handling, matching Phase 20's Vulkan behaviour.
 *
 * ARC is enabled for this translation unit via `-fobjc-arc` (set in
 * `Backends/SDL3Metal/CMakeLists.txt`) — no manual retain/release/autorelease
 * calls appear here, and no `@autoreleasepool` inside BeginFrame()/EndFrame().
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "SDL3MetalBackend.hpp"
#include "../SDL3Vulkan/InputTranslation.hpp"
#include "ViewportMetal.hpp"

#include <imgui.h>
#include <imgui_impl_metal.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <string>
#include <utility>

namespace ImFrame::Internal {

namespace {

constexpr float DELTA_TIME_MAX     = 0.1f; // cap to prevent physics blow-up
constexpr float DEFAULT_DELTA_TIME = 1.0f / 60.0f;
constexpr float CLEAR_R            = 0.06f;
constexpr float CLEAR_G            = 0.06f;
constexpr float CLEAR_B            = 0.06f;

float ClampDelta(float dt) noexcept
{
    if (dt < 0.0f)           return 0.0f;
    if (dt > DELTA_TIME_MAX) return DELTA_TIME_MAX;
    return dt;
}

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

SDL3MetalBackend::~SDL3MetalBackend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult SDL3MetalBackend::Init(const WindowConfig& config)
{
    if (_initialised) return std::unexpected(Error::AlreadyInitialised);
    if (config.Width <= 0 || config.Height <= 0) return std::unexpected(Error::InvalidArgument);

    _framesInFlight  = (config.FramesInFlight > 0) ? config.FramesInFlight : 2;
    _primary.config  = config;

    if (auto r = InitSDL(config); !r) return r;

    // Window is always created hidden — shown only for the non-headless path
    // once every resource is ready, avoiding a visible flash of an unconfigured
    // surface (matches SDL3VulkanBackend's sequencing).
    SDL_WindowFlags wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (!_primaryIsHeadless) wflags |= SDL_WINDOW_METAL;
    _primary.sdlWindow = SDL_CreateWindow(std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!_primary.sdlWindow) {
        Shutdown();
        return std::unexpected(Error::WindowCreationFailed);
    }

    if (auto r = CreateDevice(); !r) { Shutdown(); return r; }
    if (auto r = CreateMetalLayerFor(_primary, _primaryIsHeadless); !r) { Shutdown(); return r; }
    if (auto r = AllocateFrameResources(_primary); !r) { Shutdown(); return r; }
    if (auto r = InitImGui(config); !r) { Shutdown(); return r; }

    if (!_primaryIsHeadless) SDL_ShowWindow(_primary.sdlWindow);

    SDL_WindowID primaryId = SDL_GetWindowID(_primary.sdlWindow);
    _sdlIdToHandle[primaryId] = PrimaryWindow;

    _lastPerfCount = SDL_GetPerformanceCounter();
    _initialised   = true;
    return {};
}

// ─── InitSDL ──────────────────────────────────────────────────────────────────

VoidResult SDL3MetalBackend::InitSDL(const WindowConfig& /*config*/)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return std::unexpected(Error::WindowCreationFailed);
    }
    _sdlInitialised = true;
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    return {};
}

// ─── CreateDevice ─────────────────────────────────────────────────────────────

VoidResult SDL3MetalBackend::CreateDevice()
{
    _device = MTLCreateSystemDefaultDevice();
    if (!_device) return std::unexpected(Error::GraphicsInitFailed);

    _commandQueue = [_device newCommandQueue];
    if (!_commandQueue) return std::unexpected(Error::GraphicsInitFailed);
    _commandQueue.label = @"ImFrame Command Queue";

    return {};
}

// ─── CreateMetalLayerFor ──────────────────────────────────────────────────────

VoidResult SDL3MetalBackend::CreateMetalLayerFor(WindowData& wd, bool headless)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);

    CAMetalLayer* layer = nil;
    if (!headless) {
        wd.metalView = SDL_Metal_CreateView(wd.sdlWindow);
        if (!wd.metalView) return std::unexpected(Error::GraphicsInitFailed);
        layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(wd.metalView);
        if (!layer) return std::unexpected(Error::GraphicsInitFailed);
    }

    MetalLayerDesc desc{};
    desc.device         = _device;
    desc.layer          = layer;
    desc.vsyncMode      = wd.config.VSync;
    desc.hdrOutput      = wd.config.HDROutput;
    desc.framesInFlight = _framesInFlight;
    desc.drawableWidth  = w;
    desc.drawableHeight = h;
    desc.headless       = headless;

    return wd.metalLayer.Create(desc);
}

// ─── AllocateFrameResources ───────────────────────────────────────────────────

VoidResult SDL3MetalBackend::AllocateFrameResources(WindowData& wd)
{
    wd.frames.resize(static_cast<std::size_t>(_framesInFlight));

    wd.frameSemaphore = dispatch_semaphore_create(_framesInFlight);
    if (!wd.frameSemaphore) return std::unexpected(Error::GraphicsInitFailed);

    return {};
}

// ─── InitImGui ────────────────────────────────────────────────────────────────

VoidResult SDL3MetalBackend::InitImGui(const WindowConfig& config)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (config.Docking)   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (config.Viewports) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForMetal(_primary.sdlWindow)) {
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    if (!ImGui_ImplMetal_Init(_device)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    return {};
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo SDL3MetalBackend::Poll()
{
    IMF_ASSERT(_initialised);

    namespace IT = InputTranslation;

    // The only autorelease pool in the per-frame path — collects Objective-C
    // temporaries created while draining SDL3's event queue. Never used inside
    // BeginFrame()/EndFrame() (Phase 21 proposal invariant).
    @autoreleasepool {
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

                case SDL_EVENT_WINDOW_RESIZED: {
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
    }

    // ── Delta time ────────────────────────────────────────────────────────────
    uint64_t now  = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    float dt = (_lastPerfCount > 0 && freq > 0)
                   ? ClampDelta(static_cast<float>(now - _lastPerfCount) / static_cast<float>(freq))
                   : DEFAULT_DELTA_TIME;
    _lastPerfCount = now;

    // ── Display refresh interval ──────────────────────────────────────────────
    float refreshInterval = DEFAULT_DELTA_TIME;
    SDL_DisplayID displayId = SDL_GetDisplayForWindow(_primary.sdlWindow);
    if (displayId != 0) {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
        if (mode && mode->refresh_rate > 0.0f) {
            refreshInterval = 1.0f / mode->refresh_rate;
        }
    }

    // ── Active windows ────────────────────────────────────────────────────────
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

void SDL3MetalBackend::BeginFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    if (wd.needsResize) {
        HandleResize(wd);
        wd.needsResize = false;
    }

    // Skip frame if a windowed surface has zero area (minimized/occluded).
    // Headless layers have no OS window area concept and are never skipped.
    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &pw, &ph);
    if (!wd.metalLayer.headless && (pw == 0 || ph == 0)) {
        wd.frameStarted = false;
        ImGui_ImplMetal_NewFrame(wd.metalLayer.renderPassDescriptor);
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return;
    }

    // Bounds the CPU at most `framesInFlight` frames ahead of the GPU.
    dispatch_semaphore_wait(wd.frameSemaphore, DISPATCH_TIME_FOREVER);

    if (!wd.metalLayer.Acquire()) {
        // nextDrawable returned nil — memory pressure or an occluded window.
        // Skip the frame without submitting GPU work, but give back the
        // semaphore slot consumed above since no completion handler will fire.
        dispatch_semaphore_signal(wd.frameSemaphore);
        wd.frameStarted = false;
        ImGui_ImplMetal_NewFrame(wd.metalLayer.renderPassDescriptor);
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return;
    }

    wd.frameStarted = true;

    MTLClearColor clearColor = MTLClearColorMake(CLEAR_R, CLEAR_G, CLEAR_B, 1.0);
    wd.metalLayer.UpdateRenderPassAttachment(MTLLoadActionClear, clearColor);

    ImGui_ImplMetal_NewFrame(wd.metalLayer.renderPassDescriptor);
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void SDL3MetalBackend::EndFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    ImGui::Render();

    if (!wd.frameStarted) {
        ImGui::EndFrame();
        return;
    }

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

    id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:wd.metalLayer.renderPassDescriptor];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, encoder);
    [encoder endEncoding];

    // ReadPixels() support — headless only (see DECISIONS.md for the windowed
    // capture scoping decision). On Apple Silicon (`hasUnifiedMemory`) no blit
    // is needed: ReadPixels() reads `headlessTexture` directly via `getBytes:`.
    // On a discrete GPU, blit into the Managed-storage readback texture now,
    // on the same command buffer, before this frame's work is committed.
    if (handle == PrimaryWindow && wd.metalLayer.headless && wd.metalLayer.headlessTexture &&
        !_device.hasUnifiedMemory) {
        id<MTLTexture> source = wd.metalLayer.headlessTexture;
        if (EnsureReadbackTexture(source.width, source.height, source.pixelFormat)) {
            id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
            [blit copyFromTexture:source
                       sourceSlice:0
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:MTLSizeMake(source.width, source.height, 1)
                         toTexture:_readbackTexture
                  destinationSlice:0
                  destinationLevel:0
                 destinationOrigin:MTLOriginMake(0, 0, 0)];
            [blit synchronizeResource:_readbackTexture];
            [blit endEncoding];
        }
    }

    wd.metalLayer.Present(commandBuffer);

    dispatch_semaphore_t sem = wd.frameSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> /*buf*/) {
        dispatch_semaphore_signal(sem);
    }];

    if (handle == PrimaryWindow && wd.metalLayer.headless) {
        _lastHeadlessCommandBuffer = commandBuffer;
    }

    [commandBuffer commit];

    wd.frameIndex   = (wd.frameIndex + 1) % static_cast<uint32_t>(_framesInFlight);
    wd.frameStarted = false;
}

// ─── HandleResize ─────────────────────────────────────────────────────────────

void SDL3MetalBackend::HandleResize(WindowData& wd)
{
    if (wd.metalLayer.headless) return; // offscreen target size is fixed at creation

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);
    if (w == 0 || h == 0) return; // still minimized

    wd.metalLayer.Resize(w, h);
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void SDL3MetalBackend::Shutdown()
{
    // Guards on _sdlInitialised, not (_initialised || _device != nil) — Init()
    // calls Shutdown() on every failure path from SDL_CreateWindow() onward,
    // including when SDL_CreateWindow() or CreateDevice() itself fails, at
    // which point _initialised is still false and _device is still nil even
    // though SDL_Init() already succeeded and needs a matching SDL_Quit().
    if (!_sdlInitialised) return;

    // Destroy secondary windows.
    for (auto& [_, wd] : _secondaryWindows) {
        wd.metalLayer.Destroy();
        wd.frameSemaphore = nullptr;
        if (wd.metalView) {
            SDL_Metal_DestroyView(wd.metalView);
            wd.metalView = nullptr;
        }
        if (wd.sdlWindow) SDL_DestroyWindow(wd.sdlWindow);
    }
    _secondaryWindows.clear();
    _sdlIdToHandle.clear();

    if (_device != nil) {
        ImGui_ImplMetal_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    _primary.metalLayer.Destroy();
    _primary.frames.clear();
    _primary.frameSemaphore = nullptr;
    if (_primary.metalView) {
        SDL_Metal_DestroyView(_primary.metalView);
        _primary.metalView = nullptr;
    }
    if (_primary.sdlWindow) {
        SDL_DestroyWindow(_primary.sdlWindow);
        _primary.sdlWindow = nullptr;
    }

    // ARC releases these once the last reference drops — no manual call needed.
    _readbackTexture           = nil;
    _lastHeadlessCommandBuffer = nil;
    _commandQueue               = nil;
    _device                     = nil;

    SDL_Quit();

    _initialised    = false;
    _sdlInitialised = false;
    _shouldClose    = false;
    _lastPerfCount  = 0;
    _inputQueue.clear();
    _drainBuffer.clear();
}

// ─── NativeHandle ─────────────────────────────────────────────────────────────

void* SDL3MetalBackend::NativeHandle() const
{
    return _primary.sdlWindow;
}

// ─── CancelClose ──────────────────────────────────────────────────────────────

void SDL3MetalBackend::CancelClose() noexcept
{
    _shouldClose = false;
}

// ─── WindowDpiScale ───────────────────────────────────────────────────────────

float SDL3MetalBackend::WindowDpiScale(WindowHandle handle) const
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

WindowExtent SDL3MetalBackend::WindowSize(WindowHandle handle) const
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

bool SDL3MetalBackend::WindowIsMinimized(WindowHandle handle) const
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

bool SDL3MetalBackend::WindowIsFocused(WindowHandle handle) const
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

WindowHandle SDL3MetalBackend::CreateWindow(const WindowConfig& config)
{
    IMF_ASSERT(_initialised);

    WindowData wd{};
    wd.config = config;

    SDL_WindowFlags wflags = SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    wd.sdlWindow = SDL_CreateWindow(std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!wd.sdlWindow) return PrimaryWindow;

    if (!CreateMetalLayerFor(wd, false) || !AllocateFrameResources(wd)) {
        wd.metalLayer.Destroy();
        if (wd.metalView) SDL_Metal_DestroyView(wd.metalView);
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

void SDL3MetalBackend::DestroyWindow(WindowHandle handle)
{
    if (handle == PrimaryWindow) return;

    auto it = _secondaryWindows.find(handle);
    if (it == _secondaryWindows.end()) return;

    WindowData& wd = it->second;
    wd.metalLayer.Destroy();
    wd.frameSemaphore = nullptr;
    if (wd.metalView) {
        SDL_Metal_DestroyView(wd.metalView);
        wd.metalView = nullptr;
    }
    if (wd.sdlWindow) {
        SDL_WindowID sdlId = SDL_GetWindowID(wd.sdlWindow);
        _sdlIdToHandle.erase(sdlId);
        SDL_DestroyWindow(wd.sdlWindow);
    }

    _secondaryWindows.erase(it);
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> SDL3MetalBackend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext SDL3MetalBackend::GetNativeGraphicsContext() const
{
    return MetalContext{
        .Device         = (__bridge void*)_device,
        .CommandQueue   = (__bridge void*)_commandQueue,
        .PixelFormat    = static_cast<uint32_t>(_primary.metalLayer.pixelFormat),
        .FramesInFlight = _framesInFlight,
    };
}

// ─── CreateViewportFramebuffer ────────────────────────────────────────────────

std::unique_ptr<IViewportFramebuffer> SDL3MetalBackend::CreateViewportFramebuffer(
    std::uint32_t width, std::uint32_t height)
{
    if (!_initialised || _device == nil) return nullptr;
    return std::make_unique<ViewportFramebufferMetal>(
        _device,
        _commandQueue,
        MTLPixelFormatBGRA8Unorm_sRGB,
        width,
        height);
}

// ─── ReadPixels ───────────────────────────────────────────────────────────────

std::vector<std::byte> SDL3MetalBackend::ReadPixels() const
{
    if (!_initialised || _device == nil) return {};
    if (!_primary.metalLayer.headless) return {}; // windowed capture not implemented — see DECISIONS.md

    id<MTLTexture> tex = _primary.metalLayer.headlessTexture;
    if (!tex) return {};

    const std::size_t width  = tex.width;
    const std::size_t height = tex.height;
    if (width == 0 || height == 0) return {};

    // EndFrame() already recorded this frame's render (and, on a discrete GPU,
    // the readback blit) onto this command buffer; wait for it to finish
    // before reading. Debug/test utility, not a hot-path call.
    if (_lastHeadlessCommandBuffer != nil) {
        [_lastHeadlessCommandBuffer waitUntilCompleted];
    }

    const std::size_t bytesPerRow = width * 4;
    std::vector<std::byte> pixels(bytesPerRow * height);
    MTLRegion region = MTLRegionMake2D(0, 0, width, height);

    if (_device.hasUnifiedMemory) {
        [tex getBytes:pixels.data() bytesPerRow:bytesPerRow fromRegion:region mipmapLevel:0];
    } else {
        if (!_readbackTexture) return {}; // blit was never run (first frame raced ReadPixels())
        [_readbackTexture getBytes:pixels.data() bytesPerRow:bytesPerRow fromRegion:region mipmapLevel:0];
    }

    const MTLPixelFormat format = tex.pixelFormat;
    const bool isBgra = (format == MTLPixelFormatBGRA8Unorm_sRGB || format == MTLPixelFormatBGRA8Unorm);
    const bool isRgba = (format == MTLPixelFormatRGBA8Unorm_sRGB || format == MTLPixelFormatRGBA8Unorm);
    if (!isBgra && !isRgba) return {}; // HDR (16-bit float) formats not yet supported

    if (isBgra) {
        // Headless texture stores BGRA — swizzle to RGBA8 to match the
        // documented contract (mirrors SDL3VulkanBackend::ReadPixels()).
        for (std::size_t i = 0; i + 2 < pixels.size(); i += 4) {
            std::swap(pixels[i], pixels[i + 2]);
        }
    }

    return pixels;
}

// ─── EnsureReadbackTexture ─────────────────────────────────────────────────────

bool SDL3MetalBackend::EnsureReadbackTexture(std::size_t width, std::size_t height, MTLPixelFormat format)
{
    if (_readbackTexture != nil && _readbackTexture.width == width && _readbackTexture.height == height &&
        _readbackTexture.pixelFormat == format) {
        return true;
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                             width:static_cast<NSUInteger>(width)
                                                            height:static_cast<NSUInteger>(height)
                                                         mipmapped:NO];
    desc.usage       = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    _readbackTexture = [_device newTextureWithDescriptor:desc];
    return _readbackTexture != nil;
}

// ─── FindWindow ───────────────────────────────────────────────────────────────

SDL3MetalBackend::WindowData* SDL3MetalBackend::FindWindow(WindowHandle handle)
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

const SDL3MetalBackend::WindowData* SDL3MetalBackend::FindWindow(WindowHandle handle) const
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

WindowHandle SDL3MetalBackend::HandleForSDLWindow(SDL_WindowID id) const
{
    auto it = _sdlIdToHandle.find(id);
    return (it != _sdlIdToHandle.end()) ? it->second : PrimaryWindow;
}

} // namespace ImFrame::Internal
