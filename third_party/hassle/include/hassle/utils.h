#pragma once

#include "hassle/os.h"
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

namespace hassle {

// Forward declarations
struct DxcBlob;
struct DxcBlobEncoding;
struct DxcOperationResult;

// Error types
class HassleError : public std::runtime_error {
public:
    explicit HassleError(const std::string& message)
        : std::runtime_error(message) {}
};

class OperationError : public HassleError {
public:
    HResult hr;
    std::string message;

    OperationError(HResult hr, const std::string& msg)
        : HassleError("Dxc error " + std::to_string(hr.value) + ": " + msg)
        , hr(hr)
        , message(msg) {}
};

class Win32Error : public HassleError {
public:
    HResult hr;

    explicit Win32Error(HResult hr)
        : HassleError("Win32 error: " + std::to_string(hr.value))
        , hr(hr) {}
};

class LoadLibraryError : public HassleError {
public:
    std::string filename;

    LoadLibraryError(const std::string& file, const std::string& details)
        : HassleError("Failed to load library " + file + ": " + details)
        , filename(file) {}
};

// Result type
template<typename T>
using Result = T; // In C++, we'll use exceptions instead of Result<T, E>

// String conversion utilities
std::wstring ToWide(const std::string& str);
std::string FromWide(const wchar_t* wide);
std::string FromBstr(BSTR bstr);
std::string FromLpstr(LPCSTR lpstr);

// Helper to check HRESULT and throw exception if failed
inline void CheckHResult(HResult hr) {
    if (hr.IsError()) {
        throw Win32Error(hr);
    }
}

template<typename T>
inline T CheckHResultWithValue(HResult hr, T&& value) {
    CheckHResult(hr);
    return std::forward<T>(value);
}

// Operation output structure
struct OperationOutput {
    std::optional<std::string> messages; // Warnings/errors from DXC
    std::vector<uint8_t> blob;           // Compiled shader blob

    static OperationOutput FromOperationResult(const DxcOperationResult& result);
};

// High-level compilation function
OperationOutput CompileHlsl(
    const std::string& sourceName,
    const std::string& shaderText,
    const std::string& entryPoint,
    const std::string& targetProfile,
    const std::vector<std::string>& args,
    const std::vector<std::pair<std::string, std::optional<std::string>>>& defines);

// DXIL validation function (Windows only)
OperationOutput ValidateDxil(const std::vector<uint8_t>& data);

// Fake signing (cross-platform alternative to validation)
bool FakeSignDxilInPlace(std::vector<uint8_t>& dxil);

} // namespace hassle
