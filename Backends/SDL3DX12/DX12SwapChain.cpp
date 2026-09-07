/**
 * @file     DX12SwapChain.cpp
 * @brief    DX12SwapChain::Create/Destroy/Resize, HDR output detection
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "DX12SwapChain.hpp"
#include "DX12Util.hpp"

#include <string>

namespace ImFrame::Internal {

namespace {

// Conservative placeholder HDR10 metadata (Rec.2020 primaries, D65 white
// point, 1000-nit mastering display) — refining this with real content
// metadata is future work; DXGI requires *some* metadata to be set before
// presenting to an HDR10 swap chain.
constexpr DXGI_HDR_METADATA_HDR10 CONSERVATIVE_HDR10_METADATA = {
    .RedPrimary               = { 35400, 14600 },
    .GreenPrimary             = { 8500, 39850 },
    .BluePrimary              = { 6550, 2300 },
    .WhitePoint                = { 15635, 16450 },
    .MaxMasteringLuminance     = 10000000, // 1000 nits, in units of 0.0001 nits
    .MinMasteringLuminance     = 10,       // 0.001 nits, in units of 0.0001 nits
    .MaxContentLightLevel      = 0,        // unknown — conservative
    .MaxFrameAverageLightLevel = 0,        // unknown — conservative
};

// Finds the IDXGIOutput6 containing `hwnd`'s monitor and checks whether it
// reports an HDR-capable color space. Returns DXGI_COLOR_SPACE_RGB_FULL_G709
// (i.e. "not HDR") if anything along the way fails — HDR is opt-in, never assumed.
DXGI_COLOR_SPACE_TYPE QueryOutputColorSpace(IDXGIFactory4* factory, HWND hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; ++j) {
            DXGI_OUTPUT_DESC outDesc{};
            if (FAILED(output->GetDesc(&outDesc)) || outDesc.Monitor != monitor) {
                output.Reset();
                continue;
            }

            Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
            if (FAILED(output.As(&output6))) return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

            DXGI_OUTPUT_DESC1 desc1{};
            if (FAILED(output6->GetDesc1(&desc1))) return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            return desc1.ColorSpace;
        }
        adapter.Reset();
    }
    return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
}

} // anonymous namespace

// ─── Create ───────────────────────────────────────────────────────────────────

VoidResult DX12SwapChain::Create(const DX12SwapChainDesc& desc)
{
    rtvAllocator   = desc.rtvAllocator;
    device         = desc.device;
    width          = static_cast<UINT>(desc.drawableWidth);
    height         = static_cast<UINT>(desc.drawableHeight);
    bufferCount    = static_cast<UINT>(desc.framesInFlight) + 1;
    tearingEnabled = desc.tearingSupported && desc.vsyncMode != VSyncMode::On;
    format         = DXGI_FORMAT_B8G8R8A8_UNORM;

    bool useHdr = false;
    if (desc.hdrOutput) {
        DXGI_COLOR_SPACE_TYPE colorSpace = QueryOutputColorSpace(desc.factory, desc.hwnd);
        useHdr = (colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) ||
                 (colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
        if (useHdr) format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.Width            = width;
    scDesc.Height           = height;
    scDesc.Format           = format;
    scDesc.Stereo           = FALSE;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount      = bufferCount;
    scDesc.Scaling          = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.AlphaMode        = DXGI_ALPHA_MODE_UNSPECIFIED;
    scDesc.Flags            = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (tearingEnabled) scDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(desc.factory->CreateSwapChainForHwnd(desc.directQueue, desc.hwnd, &scDesc, nullptr, nullptr,
                                                     &swapChain1))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    if (FAILED(swapChain1.As(&handle))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    handle->SetMaximumFrameLatency(static_cast<UINT>(desc.framesInFlight));
    frameLatencyWaitableObject = handle->GetFrameLatencyWaitableObject();

    if (useHdr) {
        handle->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
        handle->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(CONSERVATIVE_HDR10_METADATA),
                                const_cast<DXGI_HDR_METADATA_HDR10*>(&CONSERVATIVE_HDR10_METADATA));
    }

    return CreateBackBufferViews();
}

// ─── CreateBackBufferViews ────────────────────────────────────────────────────

VoidResult DX12SwapChain::CreateBackBufferViews()
{
    buffers.resize(bufferCount);
    rtvIndices.resize(bufferCount);

    for (UINT i = 0; i < bufferCount; ++i) {
        if (FAILED(handle->GetBuffer(i, IID_PPV_ARGS(&buffers[i])))) {
            return std::unexpected(Error::GraphicsInitFailed);
        }

        if (!rtvAllocator->Allocate(rtvIndices[i])) {
            return std::unexpected(Error::GraphicsInitFailed);
        }
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format        = format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(buffers[i].Get(), &rtvDesc, rtvAllocator->CpuHandle(rtvIndices[i]));

        std::string tag = "ImFrame SwapChain Buffer " + std::to_string(i);
        SetD3D12DebugName(buffers[i].Get(), tag);
    }
    return {};
}

// ─── Destroy ──────────────────────────────────────────────────────────────────

void DX12SwapChain::Destroy()
{
    ReleaseBackBufferViews();
    handle.Reset();
    // frameLatencyWaitableObject is owned by the swap chain — released with it, never CloseHandle'd here.
    frameLatencyWaitableObject = nullptr;
    bufferCount = 0;
    width       = 0;
    height      = 0;
}

// ─── ReleaseBackBufferViews ───────────────────────────────────────────────────

void DX12SwapChain::ReleaseBackBufferViews()
{
    if (rtvAllocator) {
        for (UINT idx : rtvIndices) rtvAllocator->Free(idx);
    }
    rtvIndices.clear();
    buffers.clear();
}

// ─── Resize ───────────────────────────────────────────────────────────────────

VoidResult DX12SwapChain::Resize(UINT newWidth, UINT newHeight)
{
    ReleaseBackBufferViews();

    // ResizeBuffers()'s SwapChainFlags must reproduce every flag the swap
    // chain was originally created with — DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
    // cannot be added or removed by ResizeBuffers (confirmed via the DXGI
    // debug layer: "Cannot add or remove the DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
    // flag using ResizeBuffers"), unlike DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING.
    UINT flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (tearingEnabled) flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    if (FAILED(handle->ResizeBuffers(bufferCount, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, flags))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    width  = newWidth;
    height = newHeight;
    return CreateBackBufferViews();
}

// ─── CurrentRtv ───────────────────────────────────────────────────────────────

D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::CurrentRtv() const noexcept
{
    UINT index = handle->GetCurrentBackBufferIndex();
    return rtvAllocator->CpuHandle(rtvIndices[index]);
}

// ─── CurrentBuffer ────────────────────────────────────────────────────────────

ID3D12Resource* DX12SwapChain::CurrentBuffer() const noexcept
{
    UINT index = handle->GetCurrentBackBufferIndex();
    return buffers[index].Get();
}

} // namespace ImFrame::Internal
