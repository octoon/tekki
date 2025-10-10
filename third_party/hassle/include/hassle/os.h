#pragma once

#include <cstdint>

// Platform abstraction layer for Windows types
// Allows compilation on both Windows and non-Windows platforms

#ifdef _WIN32
    #include <Windows.h>
    #include <wtypes.h>
    #include <oleauto.h>
    #include <combaseapi.h>
#else
    #include <cstdlib>
    #include <wchar.h>

namespace hassle {
namespace os {

    using CHAR = char;
    using UINT = uint32_t;
    using WCHAR = wchar_t;
    using OLECHAR = WCHAR;
    using LPSTR = CHAR*;
    using LPWSTR = WCHAR*;
    using LPCSTR = const CHAR*;
    using LPCWSTR = const WCHAR*;
    using BSTR = OLECHAR*;
    using LPBSTR = BSTR*;
    using HRESULT = int32_t;

    // Returns a mutable pointer to the length prefix of the string
    inline UINT* LenPtr(BSTR p) {
        // The first four bytes before the pointer contain the length prefix:
        // https://docs.microsoft.com/en-us/previous-versions/windows/desktop/automat/bstr#remarks
        return reinterpret_cast<UINT*>(p) - 1;
    }

    // Free memory allocated with malloc
    inline void CoTaskMemFree(void* p) {
        // https://github.com/microsoft/DirectXShaderCompiler/blob/56e22b30c5e43632f56a1f97865f37108ec35463/include/dxc/Support/WinAdapter.h#L46
        if (p != nullptr) {
            std::free(p);
        }
    }

    // Free a BSTR string
    inline void SysFreeString(BSTR p) {
        // https://github.com/microsoft/DirectXShaderCompiler/blob/56e22b30c5e43632f56a1f97865f37108ec35463/lib/DxcSupport/WinAdapter.cpp#L50-L53
        if (p != nullptr) {
            std::free(LenPtr(p));
        }
    }

    // Returns the size of p in bytes, excluding terminating NULL character
    inline UINT SysStringByteLen(BSTR p) {
        if (p == nullptr) {
            return 0;
        }
        return *LenPtr(p);
    }

    // Returns the size of p in characters, excluding terminating NULL character
    inline UINT SysStringLen(BSTR p) {
        return SysStringByteLen(p) / sizeof(OLECHAR);
    }

} // namespace os
} // namespace hassle

// Import into global namespace for consistency
using namespace hassle::os;

#endif // !_WIN32

namespace hassle {

// Wrapper for HRESULT with error checking
struct HResult {
    int32_t value;

    constexpr HResult(int32_t v = 0) : value(v) {}

    constexpr bool IsError() const {
        return value < 0;
    }

    constexpr bool IsSuccess() const {
        return !IsError();
    }

    constexpr operator int32_t() const {
        return value;
    }
};

} // namespace hassle
