/**
 * @file     DX12Leak_test.cpp
 * @brief    Debug-only leak check: full init-render-shutdown cycle must report no D3D12 errors
 *
 * Rather than parsing `ReportLiveDeviceObjects()`'s text output (it writes to
 * the debug message stream, not a return value), this test registers its own
 * `ID3D12InfoQueue1` message callback directly on the backend's device —
 * D3D12 supports multiple simultaneously-registered callbacks on the same
 * `ID3D12InfoQueue1`, invoked synchronously on the calling thread, so this is
 * deterministic and independent of `SDL3DX12Backend`'s own internal callback
 * (registered separately in `CreateDeviceAndQueues()`) or of ImFrame's
 * `Logger`'s async dispatch timing. Any ERROR/CORRUPTION-severity message
 * produced during the cycle — including by `ReportLiveDeviceObjects()`
 * itself if it finds unexpected live objects — fails the test.
 *
 * Compiled out entirely in Release: the debug layer (and therefore
 * `ID3D12InfoQueue1`) does not exist outside debug builds.
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

#if !defined(NDEBUG)

#include "Backends/SDL3DX12/SDL3DX12Backend.hpp"

#include <imgui.h>

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <d3d12.h>
#include <wrl/client.h>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {

WindowConfig LeakTestWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "DX12Leak_test";
    cfg.Width  = 160;
    cfg.Height = 120;
    return cfg;
}

void TestMessageCallback(D3D12_MESSAGE_CATEGORY /*category*/, D3D12_MESSAGE_SEVERITY severity,
                          D3D12_MESSAGE_ID /*id*/, LPCSTR /*description*/, void* context)
{
    // CORRUPTION = 0, ERROR = 1 in D3D12_MESSAGE_SEVERITY's declared order —
    // anything at or below ERROR is a genuine problem, not routine reporting.
    if (severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
        ++*static_cast<std::atomic<int>*>(context);
    }
}

} // namespace

TEST_CASE("SDL3DX12Backend full init-render-shutdown cycle reports no D3D12 errors", "[dx12]")
{
    std::atomic<int> errorCount{ 0 };

    {
        SDL3DX12Backend backend;
        REQUIRE(backend.Init(LeakTestWindowConfig()).has_value());

        NativeGraphicsContext ctx = backend.GetNativeGraphicsContext();
        REQUIRE(std::holds_alternative<DX12Context>(ctx));
        auto* device = static_cast<ID3D12Device*>(std::get<DX12Context>(ctx).Device);
        REQUIRE(device != nullptr);

        Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue;
        REQUIRE(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))));

        DWORD cookie = 0;
        REQUIRE(SUCCEEDED(infoQueue->RegisterMessageCallback(&TestMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                              &errorCount, &cookie)));

        for (int i = 0; i < 5; ++i) {
            backend.Poll();
            backend.BeginFrame();
            ImGui::Begin("Leak");
            ImGui::Text("Frame %d", i);
            ImGui::End();
            backend.EndFrame();
        }

        // Shutdown() itself calls ReportLiveDeviceObjects() before releasing
        // the device — any leak it finds surfaces through this callback.
        backend.Shutdown();

        infoQueue->UnregisterMessageCallback(cookie);
        // infoQueue going out of scope below releases the test's own
        // reference to the device; it was the only thing keeping the
        // already-Shutdown() device's refcount above zero.
    }

    REQUIRE(errorCount.load() == 0);
}

#endif // !defined(NDEBUG)
