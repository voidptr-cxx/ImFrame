/**
 * @file     SDL3DX12Backend.cpp
 * @brief    SDL3 + DirectX 12 backend — Init, Poll, BeginFrame, EndFrame, Shutdown
 *
 * @internal
 * Initialisation sequence:
 *   ApplyDpiAwareness → InitSDL → create SDL window (hidden) → EnableDebugLayerIfDebug →
 *   CreateFactoryAndAdapter → CreateDeviceAndQueues → CreateDescriptorHeaps →
 *   CreateSwapChainFor(primary) → AllocateFrameResources(primary) → InitImGui →
 *   show window (windowed mode only).
 *
 * `SrvDescriptorAllocFn`/`SrvDescriptorFreeFn` (passed to `ImGui_ImplDX12_Init()`)
 * route through `_srvHeap`, the same shared `DescriptorAllocator` used for user
 * textures — see DECISIONS.md for why this replaced the proposal's assumed
 * single-fixed-font-descriptor pattern.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "SDL3DX12Backend.hpp"
#include "../SDL3Vulkan/InputTranslation.hpp"
#include "DX12Util.hpp"

#include "ImFrame/Utility/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl3.h>

#include <ShellScalingApi.h>
#include <SDL3/SDL_properties.h>
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

constexpr UINT MAX_WINDOWS       = 16; // RTV heap capacity sizing — see proposal's Descriptor Heaps section.
constexpr UINT SRV_HEAP_CAPACITY = 1024;

constexpr DXGI_FORMAT HEADLESS_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

float ClampDelta(float dt) noexcept
{
    if (dt < 0.0f)           return 0.0f;
    if (dt > DELTA_TIME_MAX) return DELTA_TIME_MAX;
    return dt;
}

constexpr UINT64 AlignUp(UINT64 value, UINT64 alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

// ─── ImGui DX12 SRV descriptor allocator callbacks ────────────────────────────
// ImGui_ImplDX12_InitInfo::UserData is set to the backend's shared SRV
// DescriptorAllocator; these free functions satisfy the callback signatures
// imgui_impl_dx12.h requires (plain function pointers, no captures allowed).

void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
                    D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
{
    auto* allocator = static_cast<DescriptorAllocator*>(info->UserData);
    UINT index = 0;
    if (!allocator->Allocate(index)) {
        // Heap exhausted — leave handles zeroed. ImGui has no recovery path
        // for this; it will skip rendering whatever texture needed the slot.
        *outCpu = D3D12_CPU_DESCRIPTOR_HANDLE{};
        *outGpu = D3D12_GPU_DESCRIPTOR_HANDLE{};
        return;
    }
    *outCpu = allocator->CpuHandle(index);
    *outGpu = allocator->GpuHandle(index);
}

void ImGuiSrvFree(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
{
    auto* allocator = static_cast<DescriptorAllocator*>(info->UserData);
    if (!allocator->heap) return;
    D3D12_CPU_DESCRIPTOR_HANDLE base = allocator->heap->GetCPUDescriptorHandleForHeapStart();
    UINT index = static_cast<UINT>((cpu.ptr - base.ptr) / allocator->descriptorSize);
    allocator->Free(index);
}

#if !defined(NDEBUG)
void DebugMessageCallback(D3D12_MESSAGE_CATEGORY /*category*/, D3D12_MESSAGE_SEVERITY severity,
                           D3D12_MESSAGE_ID /*id*/, LPCSTR description, void* /*context*/)
{
    switch (severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            IMF_FATAL("[DX12] {}", description);
            IMF_ASSERT(false); // GPU corruption is non-recoverable.
            break;
        case D3D12_MESSAGE_SEVERITY_ERROR:
            IMF_ERROR("[DX12] {}", description);
            break;
        case D3D12_MESSAGE_SEVERITY_WARNING:
            IMF_WARN("[DX12] {}", description);
            break;
        default:
            break; // INFO/MESSAGE severity not routed — too noisy for Logger.
    }
}
#endif

} // anonymous namespace

// ─── Destructor ───────────────────────────────────────────────────────────────

SDL3DX12Backend::~SDL3DX12Backend()
{
    Shutdown();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

VoidResult SDL3DX12Backend::Init(const WindowConfig& config)
{
    if (_initialised) return std::unexpected(Error::AlreadyInitialised);
    if (config.Width <= 0 || config.Height <= 0) return std::unexpected(Error::InvalidArgument);

    ApplyDpiAwareness();

    _framesInFlight = (config.FramesInFlight > 0) ? config.FramesInFlight : 2;
    _primary.config = config;

    if (auto r = InitSDL(config); !r) return r;

    // Window is always created hidden — shown only for the non-headless path
    // once every resource is ready. No SDL_WINDOW_VULKAN/_METAL-equivalent
    // flag exists for DX12 — DXGI obtains the HWND directly via SDL3's Win32
    // property and manages the surface independently of SDL3.
    SDL_WindowFlags wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    _primary.sdlWindow = SDL_CreateWindow(std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!_primary.sdlWindow) {
        Shutdown();
        return std::unexpected(Error::WindowCreationFailed);
    }

    EnableDebugLayerIfDebug();

    if (auto r = CreateFactoryAndAdapter(); !r) { Shutdown(); return r; }
    if (auto r = CreateDeviceAndQueues();   !r) { Shutdown(); return r; }
    if (auto r = CreateDescriptorHeaps();   !r) { Shutdown(); return r; }
    if (auto r = CreateSwapChainFor(_primary, _primaryIsHeadless); !r) { Shutdown(); return r; }
    if (auto r = AllocateFrameResources(_primary); !r) { Shutdown(); return r; }
    if (auto r = InitImGui(config); !r) { Shutdown(); return r; }

    if (!_primaryIsHeadless) SDL_ShowWindow(_primary.sdlWindow);

    SDL_WindowID primaryId = SDL_GetWindowID(_primary.sdlWindow);
    _sdlIdToHandle[primaryId] = PrimaryWindow;

    _lastPerfCount = SDL_GetPerformanceCounter();
    _initialised   = true;
    return {};
}

// ─── ApplyDpiAwareness ────────────────────────────────────────────────────────

void SDL3DX12Backend::ApplyDpiAwareness()
{
    // Per-monitor V2 (Windows 10 1703+) is preferred; fall back progressively.
    // A failure here is commonly because the process manifest already declared
    // DPI awareness — not fatal either way; the app just renders blurrier on
    // HiDPI displays without it.
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) return;
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE); // Windows 8.1+ fallback (shcore.lib).
}

// ─── EnableDebugLayerIfDebug ──────────────────────────────────────────────────

void SDL3DX12Backend::EnableDebugLayerIfDebug()
{
#if !defined(NDEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug1> debug1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug1)))) {
        debug1->EnableDebugLayer();
        debug1->SetEnableGPUBasedValidation(TRUE);
    }
#endif
}

// ─── InitSDL ──────────────────────────────────────────────────────────────────

VoidResult SDL3DX12Backend::InitSDL(const WindowConfig& /*config*/)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return std::unexpected(Error::WindowCreationFailed);
    }
    _sdlInitialised = true;
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    return {};
}

// ─── CreateFactoryAndAdapter ──────────────────────────────────────────────────

VoidResult SDL3DX12Backend::CreateFactoryAndAdapter()
{
    UINT flags = 0;
#if !defined(NDEBUG)
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    if (FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&_factory)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(_factory.As(&factory5))) {
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing,
                                                     sizeof(allowTearing)))) {
            _tearingSupported = (allowTearing == TRUE);
        }
    }

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    bool haveFactory6 = SUCCEEDED(_factory.As(&factory6));

    Microsoft::WRL::ComPtr<IDXGIAdapter1> best;
    int                                   bestScore = -1;

    auto considerAdapter = [&](IDXGIAdapter1* adapter) {
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) return;

        // Passing a null device pointer just checks creatability at this
        // feature level without consuming the creation or any device limit.
        if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
            return;
        }

        // DXGI has no direct discrete-vs-integrated flag (only SOFTWARE) —
        // DedicatedVideoMemory > 0 is used as a heuristic proxy, since
        // integrated GPUs typically report it as zero or near-zero.
        bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        int  score       = isSoftware ? 1 : ((desc.DedicatedVideoMemory > 0) ? 1000 : 100);
        score += static_cast<int>(desc.DedicatedVideoMemory / (1024ull * 1024ull * 1024ull));

        if (score > bestScore) {
            bestScore = score;
            best      = adapter;
        }
    };

    if (haveFactory6) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                               IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
             ++i) {
            considerAdapter(adapter.Get());
            adapter.Reset();
        }
    } else {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; _factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            considerAdapter(adapter.Get());
            adapter.Reset();
        }
    }

    if (!best) return std::unexpected(Error::GraphicsInitFailed);
    _adapter = best;
    return {};
}

// ─── CreateDeviceAndQueues ────────────────────────────────────────────────────

VoidResult SDL3DX12Backend::CreateDeviceAndQueues()
{
    if (FAILED(D3D12CreateDevice(_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&_device)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    SetD3D12DebugName(_device.Get(), "ImFrame D3D12 Device");

#if !defined(NDEBUG)
    Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue1;
    if (SUCCEEDED(_device.As(&infoQueue1))) {
        DWORD cookie = 0;
        if (SUCCEEDED(infoQueue1->RegisterMessageCallback(&DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                           nullptr, &cookie))) {
            _infoQueue       = infoQueue1;
            _infoQueueCookie = cookie;
        }
        // Specific message IDs that prove to be ImGui-related false positives
        // get added to a deny list here once discovered by running the test
        // suite on real hardware — none have been identified yet.
    }
#endif

    D3D12_COMMAND_QUEUE_DESC directDesc{};
    directDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(_device->CreateCommandQueue(&directDesc, IID_PPV_ARGS(&_directQueue)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    SetD3D12DebugName(_directQueue.Get(), "ImFrame Direct Queue");

    D3D12_COMMAND_QUEUE_DESC copyDesc{};
    copyDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    if (FAILED(_device->CreateCommandQueue(&copyDesc, IID_PPV_ARGS(&_copyQueue)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    SetD3D12DebugName(_copyQueue.Get(), "ImFrame Copy Queue");

    return {};
}

// ─── CreateDescriptorHeaps ────────────────────────────────────────────────────

VoidResult SDL3DX12Backend::CreateDescriptorHeaps()
{
    DescriptorAllocatorDesc rtvDesc{};
    rtvDesc.device        = _device.Get();
    rtvDesc.heapType      = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.capacity      = MAX_WINDOWS * static_cast<UINT>(_framesInFlight + 1);
    rtvDesc.shaderVisible = false;
    rtvDesc.debugName     = "ImFrame RTV Heap";
    if (auto r = _rtvHeap.Create(rtvDesc); !r) return r;

    DescriptorAllocatorDesc srvDesc{};
    srvDesc.device        = _device.Get();
    srvDesc.heapType      = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.capacity      = SRV_HEAP_CAPACITY;
    srvDesc.shaderVisible = true;
    srvDesc.debugName     = "ImFrame SRV Heap";
    return _srvHeap.Create(srvDesc);
}

// ─── CreateSwapChainFor ───────────────────────────────────────────────────────

VoidResult SDL3DX12Backend::CreateSwapChainFor(WindowData& wd, bool headless)
{
    wd.headless = headless;
    wd.hwnd     = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(wd.sdlWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!headless && !wd.hwnd) return std::unexpected(Error::GraphicsInitFailed);

    if (headless) {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension       = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width            = static_cast<UINT64>(wd.config.Width);
        desc.Height           = static_cast<UINT>(wd.config.Height);
        desc.DepthOrArraySize = 1;
        desc.MipLevels        = 1;
        desc.Format           = HEADLESS_FORMAT;
        desc.SampleDesc.Count = 1;
        desc.Layout            = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags             = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format   = HEADLESS_FORMAT;
        clearValue.Color[0] = CLEAR_R;
        clearValue.Color[1] = CLEAR_G;
        clearValue.Color[2] = CLEAR_B;
        clearValue.Color[3] = 1.0f;

        if (FAILED(_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                     D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
                                                     IID_PPV_ARGS(&wd.headlessTarget)))) {
            return std::unexpected(Error::GraphicsInitFailed);
        }
        SetD3D12DebugName(wd.headlessTarget.Get(), "ImFrame Headless Target");

        if (!_rtvHeap.Allocate(wd.headlessRtvIndex)) return std::unexpected(Error::GraphicsInitFailed);
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format        = HEADLESS_FORMAT;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        _device->CreateRenderTargetView(wd.headlessTarget.Get(), &rtvDesc, _rtvHeap.CpuHandle(wd.headlessRtvIndex));
        return {};
    }

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);

    DX12SwapChainDesc desc{};
    desc.factory          = _factory.Get();
    desc.device            = _device.Get();
    desc.directQueue       = _directQueue.Get();
    desc.hwnd              = wd.hwnd;
    desc.vsyncMode         = wd.config.VSync;
    desc.hdrOutput         = wd.config.HDROutput;
    desc.tearingSupported  = _tearingSupported;
    desc.framesInFlight    = _framesInFlight;
    desc.drawableWidth     = w;
    desc.drawableHeight    = h;
    desc.rtvAllocator      = &_rtvHeap;

    return wd.swapChain.Create(desc);
}

// ─── AllocateFrameResources ───────────────────────────────────────────────────

VoidResult SDL3DX12Backend::AllocateFrameResources(WindowData& wd)
{
    wd.frames.resize(static_cast<std::size_t>(_framesInFlight));
    for (auto& f : wd.frames) {
        if (auto r = f.Create(_device.Get()); !r) return r;
    }
    return {};
}

// ─── InitImGui ────────────────────────────────────────────────────────────────

VoidResult SDL3DX12Backend::InitImGui(const WindowConfig& config)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (config.Docking)   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (config.Viewports) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForD3D(_primary.sdlWindow)) {
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device                = _device.Get();
    initInfo.CommandQueue          = _directQueue.Get();
    initInfo.NumFramesInFlight     = _framesInFlight;
    initInfo.RTVFormat             = _primary.headless ? HEADLESS_FORMAT : _primary.swapChain.format;
    initInfo.DSVFormat             = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap     = _srvHeap.heap.Get();
    initInfo.SrvDescriptorAllocFn  = &ImGuiSrvAlloc;
    initInfo.SrvDescriptorFreeFn   = &ImGuiSrvFree;
    initInfo.UserData              = &_srvHeap;

    if (!ImGui_ImplDX12_Init(&initInfo)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error::GraphicsInitFailed);
    }
    return {};
}

// ─── Poll ─────────────────────────────────────────────────────────────────────

FrameInfo SDL3DX12Backend::Poll()
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

            // Physical-pixel resize, not SDL_EVENT_WINDOW_RESIZED (logical
            // pixels) — DXGI's ResizeBuffers() needs physical pixel dimensions
            // under per-monitor DPI awareness. See the proposal's Input
            // Handling section.
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

void SDL3DX12Backend::BeginFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    if (wd.needsResize) {
        HandleResize(wd);
        wd.needsResize = false;
    }

    // Skip frame if a windowed surface has zero area (minimized/occluded).
    // Headless targets have no OS window area concept and are never skipped.
    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &pw, &ph);
    if (!wd.headless && (pw == 0 || ph == 0)) {
        wd.frameStarted = false;
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return;
    }

    // Primary frame-pacing mechanism for windowed mode — replaces an
    // explicit fence wait for the common case. Headless has no swap chain
    // and therefore no waitable object; the per-slot fence wait below is its
    // only pacing mechanism.
    if (!wd.headless) {
        WaitForSingleObjectEx(wd.swapChain.frameLatencyWaitableObject, INFINITE, FALSE);
    }

    FrameResources& frame = wd.frames[wd.frameIndex % static_cast<uint32_t>(_framesInFlight)];
    // Guards against the edge case where the waitable object fires before
    // the GPU has actually finished this slot's previous frame.
    frame.fence.Wait(frame.fence.value);

    frame.commandAllocator->Reset();
    frame.commandList->Reset(frame.commandAllocator.Get(), nullptr);

    wd.frameStarted = true;

    ID3D12Resource* target = wd.headless ? wd.headlessTarget.Get() : wd.swapChain.CurrentBuffer();
    if (!wd.headless) {
        Transition(frame.commandList.Get(), target, wd.backBufferState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        wd.backBufferState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    // Headless target's state never leaves RENDER_TARGET — see CreateSwapChainFor.

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        wd.headless ? _rtvHeap.CpuHandle(wd.headlessRtvIndex) : wd.swapChain.CurrentRtv();

    UINT width  = wd.headless ? static_cast<UINT>(wd.config.Width) : static_cast<UINT>(pw);
    UINT height = wd.headless ? static_cast<UINT>(wd.config.Height) : static_cast<UINT>(ph);

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    D3D12_RECT     scissor{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    frame.commandList->RSSetViewports(1, &viewport);
    frame.commandList->RSSetScissorRects(1, &scissor);

    const float clearColor[4] = { CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };
    frame.commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    frame.commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

// ─── EndFrame ─────────────────────────────────────────────────────────────────

void SDL3DX12Backend::EndFrame(WindowHandle handle)
{
    IMF_ASSERT(_initialised);

    WindowData& wd = (handle == PrimaryWindow) ? _primary : *FindWindow(handle);
    if (!wd.sdlWindow) return;

    ImGui::Render();

    if (!wd.frameStarted) {
        ImGui::EndFrame();
        return;
    }

    FrameResources& frame = wd.frames[wd.frameIndex % static_cast<uint32_t>(_framesInFlight)];

    // SetDescriptorHeaps must be called at most once per command list — it
    // resets GPU descriptor binding state. Called immediately before
    // ImGui_ImplDX12_RenderDrawData() and never again on this list.
    ID3D12DescriptorHeap* heaps[] = { _srvHeap.heap.Get() };
    frame.commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), frame.commandList.Get());

    ID3D12Resource* target = wd.headless ? wd.headlessTarget.Get() : wd.swapChain.CurrentBuffer();

    // ReadPixels() support — headless only (see DECISIONS.md for the
    // windowed-capture scoping decision, mirroring Phase 20/21). The copy
    // happens on the same direct-queue command list as the render, before
    // the resource is transitioned back to RENDER_TARGET for the next frame.
    bool capturedForReadback = false;
    if (handle == PrimaryWindow && wd.headless) {
        UINT64 rowPitch   = AlignUp(static_cast<UINT64>(wd.config.Width) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        UINT64 bufferSize = rowPitch * static_cast<UINT64>(wd.config.Height);

        if (EnsureReadbackBuffer(bufferSize)) {
            Transition(frame.commandList.Get(), target, D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_COPY_SOURCE);

            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource        = target;
            src.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource                          = _readbackBuffer.Get();
            dst.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint.Offset             = 0;
            dst.PlacedFootprint.Footprint.Format   = HEADLESS_FORMAT;
            dst.PlacedFootprint.Footprint.Width    = static_cast<UINT>(wd.config.Width);
            dst.PlacedFootprint.Footprint.Height   = static_cast<UINT>(wd.config.Height);
            dst.PlacedFootprint.Footprint.Depth    = 1;
            dst.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);

            D3D12_BOX srcBox{ 0, 0, 0, static_cast<UINT>(wd.config.Width), static_cast<UINT>(wd.config.Height), 1 };
            frame.commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox);

            Transition(frame.commandList.Get(), target, D3D12_RESOURCE_STATE_COPY_SOURCE,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);

            _readbackRowPitch = rowPitch;
            _readbackWidth    = static_cast<UINT>(wd.config.Width);
            _readbackHeight   = static_cast<UINT>(wd.config.Height);
            capturedForReadback = true;
        }
    }

    if (!wd.headless) {
        Transition(frame.commandList.Get(), target, wd.backBufferState, D3D12_RESOURCE_STATE_PRESENT);
        wd.backBufferState = D3D12_RESOURCE_STATE_PRESENT;
    }

    frame.commandList->Close();

    ID3D12CommandList* lists[] = { frame.commandList.Get() };
    _directQueue->ExecuteCommandLists(1, lists);

    if (!wd.headless) {
        UINT syncInterval = (wd.config.VSync == VSyncMode::On) ? 1 : 0;
        UINT presentFlags = (wd.swapChain.tearingEnabled && wd.config.VSync != VSyncMode::On)
                                 ? DXGI_PRESENT_ALLOW_TEARING
                                 : 0;
        wd.swapChain.handle->Present(syncInterval, presentFlags);
    }

    frame.fence.value++;
    frame.fence.Signal(_directQueue.Get(), frame.fence.value);

    if (handle == PrimaryWindow && capturedForReadback) {
        _lastHeadlessFenceValue = frame.fence.value;
    }

    wd.frameIndex   = (wd.frameIndex + 1) % static_cast<uint32_t>(_framesInFlight);
    wd.frameStarted = false;
}

// ─── Transition ───────────────────────────────────────────────────────────────

void SDL3DX12Backend::Transition(ID3D12GraphicsCommandList7* cmdList, ID3D12Resource* resource,
                                  D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    if (from == to) return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = resource;
    barrier.Transition.StateBefore = from;
    barrier.Transition.StateAfter  = to;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
}

// ─── HandleResize ─────────────────────────────────────────────────────────────

void SDL3DX12Backend::HandleResize(WindowData& wd)
{
    if (wd.headless) return; // offscreen target size is fixed at creation

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(wd.sdlWindow, &w, &h);
    if (w == 0 || h == 0) return; // still minimized

    // ResizeBuffers() fails if any back buffer is still referenced by a
    // pending command list — drain every slot's own fence first.
    for (auto& f : wd.frames) f.fence.Wait(f.fence.value);

    if (!wd.swapChain.Resize(static_cast<UINT>(w), static_cast<UINT>(h))) {
        IMF_WARN("DX12 swap chain resize to {}x{} failed", w, h);
        return;
    }
    wd.backBufferState = D3D12_RESOURCE_STATE_PRESENT; // ResizeBuffers() yields fresh buffers in this state.
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void SDL3DX12Backend::Shutdown()
{
    // See Backends/SDL3Metal's identical fix (DECISIONS.md 2026-06-19) — guard
    // on _sdlInitialised, not _initialised/_device, since Init() calls
    // Shutdown() from failure paths before _device is ever assigned.
    if (!_sdlInitialised) return;

    auto drainWindow = [](WindowData& wd) {
        for (auto& f : wd.frames) f.fence.Wait(f.fence.value);
    };

    drainWindow(_primary);
    for (auto& [_, wd] : _secondaryWindows) drainWindow(wd);

    if (_device) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    for (auto& [_, wd] : _secondaryWindows) {
        wd.swapChain.Destroy();
        wd.frames.clear();
        if (wd.sdlWindow) SDL_DestroyWindow(wd.sdlWindow);
    }
    _secondaryWindows.clear();
    _sdlIdToHandle.clear();

    _primary.swapChain.Destroy();
    if (_primary.headlessTarget) {
        _rtvHeap.Free(_primary.headlessRtvIndex);
        _primary.headlessTarget.Reset();
    }
    _primary.frames.clear();
    if (_primary.sdlWindow) {
        SDL_DestroyWindow(_primary.sdlWindow);
        _primary.sdlWindow = nullptr;
    }

    _readbackBuffer.Reset();
    _readbackBufferSize = 0;

    _rtvHeap.Destroy();
    _srvHeap.Destroy();

#if !defined(NDEBUG)
    if (_device) {
        Microsoft::WRL::ComPtr<ID3D12DebugDevice> debugDevice;
        if (SUCCEEDED(_device.As(&debugDevice))) {
            debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        }
    }
    if (_infoQueue) {
        _infoQueue->UnregisterMessageCallback(_infoQueueCookie);
        _infoQueue.Reset();
    }
#endif

    _copyQueue.Reset();
    _directQueue.Reset();
    _device.Reset();
    _adapter.Reset();
    _factory.Reset();

    SDL_Quit();

    _initialised             = false;
    _sdlInitialised           = false;
    _shouldClose              = false;
    _lastPerfCount             = 0;
    _lastHeadlessFenceValue    = 0;
    _inputQueue.clear();
    _drainBuffer.clear();
}

// ─── NativeHandle ─────────────────────────────────────────────────────────────

void* SDL3DX12Backend::NativeHandle() const
{
    return _primary.sdlWindow;
}

// ─── CancelClose ──────────────────────────────────────────────────────────────

void SDL3DX12Backend::CancelClose() noexcept
{
    _shouldClose = false;
}

// ─── WindowDpiScale ───────────────────────────────────────────────────────────

float SDL3DX12Backend::WindowDpiScale(WindowHandle handle) const
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

WindowExtent SDL3DX12Backend::WindowSize(WindowHandle handle) const
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

bool SDL3DX12Backend::WindowIsMinimized(WindowHandle handle) const
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

bool SDL3DX12Backend::WindowIsFocused(WindowHandle handle) const
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

WindowHandle SDL3DX12Backend::CreateWindow(const WindowConfig& config)
{
    IMF_ASSERT(_initialised);

    WindowData wd{};
    wd.config = config;

    SDL_WindowFlags wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    wd.sdlWindow = SDL_CreateWindow(std::string(config.Title).c_str(), config.Width, config.Height, wflags);
    if (!wd.sdlWindow) return PrimaryWindow;

    if (!CreateSwapChainFor(wd, false) || !AllocateFrameResources(wd)) {
        wd.swapChain.Destroy();
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

void SDL3DX12Backend::DestroyWindow(WindowHandle handle)
{
    if (handle == PrimaryWindow) return;

    auto it = _secondaryWindows.find(handle);
    if (it == _secondaryWindows.end()) return;

    WindowData& wd = it->second;
    // Targets only this window's in-flight work — unlike a single
    // device-wide idle wait, this does not stall other windows' frames.
    for (auto& f : wd.frames) f.fence.Wait(f.fence.value);

    wd.swapChain.Destroy();
    wd.frames.clear();
    if (wd.sdlWindow) {
        SDL_WindowID sdlId = SDL_GetWindowID(wd.sdlWindow);
        _sdlIdToHandle.erase(sdlId);
        SDL_DestroyWindow(wd.sdlWindow);
    }

    _secondaryWindows.erase(it);
}

// ─── DrainInputEvents ─────────────────────────────────────────────────────────

std::span<const InputEvent> SDL3DX12Backend::DrainInputEvents()
{
    _drainBuffer = std::move(_inputQueue);
    _inputQueue.clear();
    return _drainBuffer;
}

// ─── GetNativeGraphicsContext ─────────────────────────────────────────────────

NativeGraphicsContext SDL3DX12Backend::GetNativeGraphicsContext() const
{
    return DX12Context{
        .Device            = static_cast<void*>(_device.Get()),
        .CommandQueue      = static_cast<void*>(_directQueue.Get()),
        .CopyQueue         = static_cast<void*>(_copyQueue.Get()),
        .SrvHeap           = static_cast<void*>(_srvHeap.heap.Get()),
        .SrvDescriptorSize = _srvHeap.descriptorSize,
        .SwapChainFormat   = static_cast<std::uint32_t>(_primary.headless ? HEADLESS_FORMAT : _primary.swapChain.format),
        .FramesInFlight    = _framesInFlight,
    };
}

// ─── ReadPixels ───────────────────────────────────────────────────────────────

std::vector<std::byte> SDL3DX12Backend::ReadPixels() const
{
    if (!_initialised || !_device) return {};
    if (!_primary.headless) return {}; // windowed capture not implemented — see DECISIONS.md
    if (!_readbackBuffer || _readbackWidth == 0 || _readbackHeight == 0) return {};

    // EndFrame() already recorded this frame's render and readback copy onto
    // the slot that last captured a frame; wait for it before reading.
    // Debug/test utility, not a hot-path call.
    uint32_t lastSlot = (_primary.frameIndex + static_cast<uint32_t>(_framesInFlight) - 1) %
                        static_cast<uint32_t>(_framesInFlight);
    _primary.frames[lastSlot].fence.Wait(_lastHeadlessFenceValue);

    void* mapped = nullptr;
    if (FAILED(_readbackBuffer->Map(0, nullptr, &mapped))) return {};

    const std::size_t tightRowBytes = static_cast<std::size_t>(_readbackWidth) * 4;
    std::vector<std::byte> pixels(tightRowBytes * _readbackHeight);

    // De-stride: the readback buffer's rows are padded to a 256-byte
    // alignment (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) — copy row by row into a
    // tightly-packed output, per the proposal's row de-striding invariant.
    const auto* src = static_cast<const std::byte*>(mapped);
    for (UINT row = 0; row < _readbackHeight; ++row) {
        std::memcpy(pixels.data() + row * tightRowBytes, src + row * _readbackRowPitch, tightRowBytes);
    }

    D3D12_RANGE writtenRange{ 0, 0 }; // CPU did not write anything back.
    _readbackBuffer->Unmap(0, &writtenRange);

    return pixels;
}

// ─── EnsureReadbackBuffer ─────────────────────────────────────────────────────

bool SDL3DX12Backend::EnsureReadbackBuffer(UINT64 size)
{
    if (_readbackBuffer && _readbackBufferSize >= size) return true;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width             = size;
    desc.Height            = 1;
    desc.DepthOrArraySize  = 1;
    desc.MipLevels         = 1;
    desc.Format            = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count  = 1;
    desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    _readbackBuffer.Reset();
    if (FAILED(_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                 IID_PPV_ARGS(&_readbackBuffer)))) {
        _readbackBufferSize = 0;
        return false;
    }
    _readbackBufferSize = size;
    SetD3D12DebugName(_readbackBuffer.Get(), "ImFrame Readback Buffer");
    return true;
}

// ─── FindWindow ───────────────────────────────────────────────────────────────

SDL3DX12Backend::WindowData* SDL3DX12Backend::FindWindow(WindowHandle handle)
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

const SDL3DX12Backend::WindowData* SDL3DX12Backend::FindWindow(WindowHandle handle) const
{
    auto it = _secondaryWindows.find(handle);
    return (it != _secondaryWindows.end()) ? &it->second : nullptr;
}

WindowHandle SDL3DX12Backend::HandleForSDLWindow(SDL_WindowID id) const
{
    auto it = _sdlIdToHandle.find(id);
    return (it != _sdlIdToHandle.end()) ? it->second : PrimaryWindow;
}

} // namespace ImFrame::Internal
