/**
 * @file     DX12Util.hpp
 * @brief    Small shared helpers for DX12 debug object naming
 *
 * `ID3D12Object::SetName()` takes `LPCWSTR`; every other backend file in this
 * directory names its objects from a `std::string_view`/`std::string`. One
 * conversion helper here avoids duplicating the UTF-8 → UTF-16 call at every
 * `SetName()` site in `DescriptorAllocator.cpp`, `DX12SwapChain.cpp`, and
 * `SDL3DX12Backend.cpp`.
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

#pragma once

#include <windows.h>
// <windows.h> #defines CreateWindow to CreateWindowA/W — collides with
// IBackend::CreateWindow(). Every header in this directory that (transitively)
// includes <windows.h> must undef it before any header declaring that method
// is parsed, since this is a blind preprocessor substitution, not scoped.
#undef CreateWindow

#include <d3d12.h>
#include <string_view>

namespace ImFrame::Internal {

/**
 * @brief   Sets a D3D12 object's debug name from a UTF-8 string view.
 *
 * No-op if `obj` is `nullptr`. Debug names are visible in PIX, the D3D12
 * debug layer's validation messages, and `ReportLiveDeviceObjects()` output.
 *
 * @param[in]  obj   Object to name. May be `nullptr`.
 * @param[in]  name  UTF-8 debug name.
 * @throws  Nothing.
 */
inline void SetD3D12DebugName(ID3D12Object* obj, std::string_view name) noexcept
{
    if (!obj) return;
    wchar_t wide[256];
    int len = MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()),
                                   wide, static_cast<int>(std::size(wide)) - 1);
    if (len <= 0) return;
    wide[len] = L'\0';
    obj->SetName(wide);
}

} // namespace ImFrame::Internal
