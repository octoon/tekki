#pragma once

#include "hassle/ffi.h"
#include "hassle/os.h"
#include "hassle/utils.h"
#include "hassle/intellisense/wrapper.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace hassle {

// Forward declarations
class DxcLibrary;

// DxcBlob wrapper
class DxcBlob {
private:
    Microsoft::WRL::ComPtr<IDxcBlob> m_blob;

public:
    DxcBlob() = default;
    explicit DxcBlob(IDxcBlob* blob) : m_blob(blob) {}

    template<typename T>
    const T* AsSlice() const {
        if (!m_blob) {
            return nullptr;
        }
        size_t bytes = m_blob->GetBufferSize();
        if (bytes == 0) {
            return nullptr;
        }
        return reinterpret_cast<const T*>(m_blob->GetBufferPointer());
    }

    template<typename T>
    std::vector<T> ToVec() const {
        auto slice = AsSlice<T>();
        if (!slice) {
            return {};
        }
        size_t count = m_blob->GetBufferSize() / sizeof(T);
        return std::vector<T>(slice, slice + count);
    }

    IDxcBlob* GetRaw() const { return m_blob.Get(); }
    bool IsValid() const { return m_blob != nullptr; }
};

// DxcBlobEncoding wrapper
enum class DxcEncoding {
    Unknown,
    Utf8
};

class DxcBlobEncoding {
private:
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> m_blob;

public:
    DxcBlobEncoding() = default;
    explicit DxcBlobEncoding(IDxcBlobEncoding* blob) : m_blob(blob) {}

    DxcEncoding GetEncoding() const {
        if (!m_blob) {
            return DxcEncoding::Unknown;
        }

        BOOL known = FALSE;
        uint32_t codePage = 0;
        HResult hr = m_blob->GetEncoding(&known, &codePage);
        CheckHResult(hr);

        if (!known) {
            return DxcEncoding::Unknown;
        } else {
            switch (codePage) {
                case HASSLE_CP_UTF8: return DxcEncoding::Utf8;
                default: throw HassleError("Unknown codepage: " + std::to_string(codePage));
            }
        }
    }

    std::optional<std::string> AsStr() const {
        auto encoding = GetEncoding();
        if (encoding != DxcEncoding::Utf8) {
            return std::nullopt;
        }

        auto slice = AsSlice<char>();
        if (!slice) {
            return std::nullopt;
        }

        return std::string(slice, m_blob->GetBufferSize());
    }

    template<typename T>
    const T* AsSlice() const {
        if (!m_blob) {
            return nullptr;
        }
        size_t bytes = m_blob->GetBufferSize();
        if (bytes == 0) {
            return nullptr;
        }
        return reinterpret_cast<const T*>(m_blob->GetBufferPointer());
    }

    operator DxcBlob() const {
        Microsoft::WRL::ComPtr<IDxcBlob> base;
        m_blob.As(&base);
        return DxcBlob(base.Get());
    }

    IDxcBlobEncoding* GetRaw() const { return m_blob.Get(); }
    bool IsValid() const { return m_blob != nullptr; }
};

// DxcOperationResult wrapper
class DxcOperationResult {
private:
    Microsoft::WRL::ComPtr<IDxcOperationResult> m_result;

public:
    DxcOperationResult() = default;
    explicit DxcOperationResult(IDxcOperationResult* result) : m_result(result) {}

    HResult GetStatus() const {
        if (!m_result) {
            throw HassleError("Invalid operation result");
        }
        HRESULT status;
        CheckHResult(m_result->GetStatus(&status));
        return HResult(status);
    }

    DxcBlob GetResult() const {
        if (!m_result) {
            throw HassleError("Invalid operation result");
        }
        Microsoft::WRL::ComPtr<IDxcBlob> blob;
        CheckHResult(m_result->GetResult(&blob));
        return DxcBlob(blob.Get());
    }

    DxcBlobEncoding GetErrorBuffer() const {
        if (!m_result) {
            throw HassleError("Invalid operation result");
        }
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob;
        CheckHResult(m_result->GetErrorBuffer(&blob));
        return DxcBlobEncoding(blob.Get());
    }

    IDxcOperationResult* GetRaw() const { return m_result.Get(); }
    bool IsValid() const { return m_result != nullptr; }
};

// Include handler interface
class DxcIncludeHandlerInterface {
public:
    virtual ~DxcIncludeHandlerInterface() = default;
    virtual std::optional<std::string> LoadSource(const std::string& filename) = 0;
};

// DxcLibrary wrapper
class DxcLibrary {
private:
    Microsoft::WRL::ComPtr<IDxcLibrary> m_library;

public:
    DxcLibrary() = default;
    explicit DxcLibrary(IDxcLibrary* library) : m_library(library) {}

    DxcBlobEncoding CreateBlobWithEncoding(const std::vector<uint8_t>& data) const {
        if (!m_library) {
            throw HassleError("Invalid library");
        }
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob;
        CheckHResult(m_library->CreateBlobWithEncodingFromPinned(
            data.data(),
            static_cast<uint32_t>(data.size()),
            0, // Binary; no code page
            &blob));
        return DxcBlobEncoding(blob.Get());
    }

    DxcBlobEncoding CreateBlobWithEncodingFromStr(const std::string& text) const {
        if (!m_library) {
            throw HassleError("Invalid library");
        }
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob;
        CheckHResult(m_library->CreateBlobWithEncodingFromPinned(
            text.data(),
            static_cast<uint32_t>(text.size()),
            HASSLE_CP_UTF8,
            &blob));
        return DxcBlobEncoding(blob.Get());
    }

    DxcBlobEncoding GetBlobAsUtf8(const DxcBlob& blob) const {
        if (!m_library) {
            throw HassleError("Invalid library");
        }
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> blobUtf8;
        CheckHResult(m_library->GetBlobAsUtf8(blob.GetRaw(), &blobUtf8));
        auto result = DxcBlobEncoding(blobUtf8.Get());
        if (result.GetEncoding() != DxcEncoding::Utf8) {
            throw HassleError("Expected UTF-8 encoding");
        }
        return result;
    }

    IDxcLibrary* GetRaw() const { return m_library.Get(); }
    bool IsValid() const { return m_library != nullptr; }
};

// DxcCompiler wrapper
class DxcCompiler {
private:
    Microsoft::WRL::ComPtr<IDxcCompiler2> m_compiler;
    DxcLibrary m_library;

    static void PrepareDefines(
        const std::vector<std::pair<std::string, std::optional<std::string>>>& defines,
        std::vector<std::wstring>& wideDefines,
        std::vector<DxcDefine>& dxcDefines)
    {
        for (const auto& [name, value] : defines) {
            std::wstring wname = ToWide(name);
            std::wstring wvalue = value ? ToWide(*value) : L"1";
            wideDefines.emplace_back(std::move(wname));
            wideDefines.emplace_back(std::move(wvalue));
        }

        for (size_t i = 0; i < defines.size(); ++i) {
            dxcDefines.push_back({
                wideDefines[i * 2].c_str(),
                wideDefines[i * 2 + 1].c_str()
            });
        }
    }

    static void PrepareArgs(
        const std::vector<std::string>& args,
        std::vector<std::wstring>& wideArgs,
        std::vector<LPCWSTR>& dxcArgs)
    {
        for (const auto& arg : args) {
            wideArgs.push_back(ToWide(arg));
        }

        for (const auto& arg : wideArgs) {
            dxcArgs.push_back(arg.c_str());
        }
    }

public:
    DxcCompiler() = default;
    DxcCompiler(IDxcCompiler2* compiler, const DxcLibrary& library)
        : m_compiler(compiler), m_library(library) {}

    DxcOperationResult Compile(
        const DxcBlobEncoding& blob,
        const std::string& sourceName,
        const std::string& entryPoint,
        const std::string& targetProfile,
        const std::vector<std::string>& args,
        DxcIncludeHandlerInterface* includeHandler,
        const std::vector<std::pair<std::string, std::optional<std::string>>>& defines) const
    {
        if (!m_compiler) {
            throw HassleError("Invalid compiler");
        }

        std::vector<std::wstring> wideArgs;
        std::vector<LPCWSTR> dxcArgs;
        PrepareArgs(args, wideArgs, dxcArgs);

        std::vector<std::wstring> wideDefines;
        std::vector<DxcDefine> dxcDefines;
        PrepareDefines(defines, wideDefines, dxcDefines);

        // TODO: Implement include handler wrapper
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler;

        Microsoft::WRL::ComPtr<IDxcOperationResult> result;
        CheckHResult(m_compiler->Compile(
            blob.GetRaw(),
            ToWide(sourceName).c_str(),
            ToWide(entryPoint).c_str(),
            ToWide(targetProfile).c_str(),
            dxcArgs.data(),
            static_cast<uint32_t>(dxcArgs.size()),
            dxcDefines.data(),
            static_cast<uint32_t>(dxcDefines.size()),
            dxcIncludeHandler.Get(),
            &result));

        return DxcOperationResult(result.Get());
    }

    DxcBlobEncoding Disassemble(const DxcBlob& blob) const {
        if (!m_compiler) {
            throw HassleError("Invalid compiler");
        }
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> result;
        CheckHResult(m_compiler->Disassemble(blob.GetRaw(), &result));
        return DxcBlobEncoding(result.Get());
    }

    IDxcCompiler2* GetRaw() const { return m_compiler.Get(); }
    bool IsValid() const { return m_compiler != nullptr; }
};

// Dxc main class
class Dxc {
private:
#ifdef _WIN32
    HMODULE m_dxcLib = nullptr;
#else
    void* m_dxcLib = nullptr;
#endif

    DxcCreateInstanceProc GetDxcCreateInstance() const {
#ifdef _WIN32
        if (!m_dxcLib) {
            throw HassleError("DXC library not loaded");
        }
        auto proc = reinterpret_cast<DxcCreateInstanceProc>(
            GetProcAddress(m_dxcLib, "DxcCreateInstance"));
        if (!proc) {
            throw HassleError("Failed to get DxcCreateInstance function");
        }
        return proc;
#else
        if (!m_dxcLib) {
            throw HassleError("DXC library not loaded");
        }
        auto proc = reinterpret_cast<DxcCreateInstanceProc>(
            dlsym(m_dxcLib, "DxcCreateInstance"));
        if (!proc) {
            throw HassleError("Failed to get DxcCreateInstance function");
        }
        return proc;
#endif
    }

public:
    Dxc() {
#ifdef _WIN32
        m_dxcLib = LoadLibraryA("dxcompiler.dll");
        if (!m_dxcLib) {
            throw LoadLibraryError("dxcompiler.dll", "Failed to load library");
        }
#else
        m_dxcLib = dlopen("libdxcompiler.so", RTLD_LAZY);
        if (!m_dxcLib) {
            throw LoadLibraryError("libdxcompiler.so", "Failed to load library");
        }
#endif
    }

    ~Dxc() {
        if (m_dxcLib) {
#ifdef _WIN32
            FreeLibrary(m_dxcLib);
#else
            dlclose(m_dxcLib);
#endif
        }
    }

    DxcCompiler CreateCompiler() const {
        auto createInstance = GetDxcCreateInstance();
        Microsoft::WRL::ComPtr<IDxcCompiler2> compiler;
        CheckHResult(createInstance(
            CLSID_DxcCompiler,
            __uuidof(IDxcCompiler2),
            &compiler));
        return DxcCompiler(compiler.Get(), CreateLibrary());
    }

    DxcLibrary CreateLibrary() const {
        auto createInstance = GetDxcCreateInstance();
        Microsoft::WRL::ComPtr<IDxcLibrary> library;
        CheckHResult(createInstance(
            CLSID_DxcLibrary,
            __uuidof(IDxcLibrary),
            &library));
        return DxcLibrary(library.Get());
    }

    // TODO: Implement CreateIntelliSense when needed
    // intellisense::DxcIntelliSense CreateIntelliSense() const;
};

// Dxil wrapper (for validation)
class Dxil {
private:
#ifdef _WIN32
    HMODULE m_dxilLib = nullptr;
#else
    void* m_dxilLib = nullptr;
#endif

    DxcCreateInstanceProc GetDxcCreateInstance() const {
#ifdef _WIN32
        if (!m_dxilLib) {
            throw HassleError("DXIL library not loaded");
        }
        auto proc = reinterpret_cast<DxcCreateInstanceProc>(
            GetProcAddress(m_dxilLib, "DxcCreateInstance"));
        if (!proc) {
            throw HassleError("Failed to get DxcCreateInstance function");
        }
        return proc;
#else
        if (!m_dxilLib) {
            throw HassleError("DXIL library not loaded");
        }
        auto proc = reinterpret_cast<DxcCreateInstanceProc>(
            dlsym(m_dxilLib, "DxcCreateInstance"));
        if (!proc) {
            throw HassleError("Failed to get DxcCreateInstance function");
        }
        return proc;
#endif
    }

public:
    Dxil() {
#ifdef _WIN32
        m_dxilLib = LoadLibraryA("dxil.dll");
        if (!m_dxilLib) {
            throw LoadLibraryError("dxil.dll", "Failed to load library");
        }
#else
        // dxil.dll is Windows-only
        throw HassleError("DXIL validation is only available on Windows");
#endif
    }

    ~Dxil() {
        if (m_dxilLib) {
#ifdef _WIN32
            FreeLibrary(m_dxilLib);
#endif
        }
    }

    class DxcValidator {
    private:
        Microsoft::WRL::ComPtr<IDxcValidator> m_validator;

    public:
        DxcValidator() = default;
        explicit DxcValidator(IDxcValidator* validator) : m_validator(validator) {}

        std::pair<uint32_t, uint32_t> GetVersion() const {
            if (!m_validator) {
                throw HassleError("Invalid validator");
            }
            Microsoft::WRL::ComPtr<IDxcVersionInfo> versionInfo;
            if (FAILED(m_validator.As(&versionInfo))) {
                throw Win32Error(E_NOINTERFACE);
            }
            uint32_t major = 0, minor = 0;
            CheckHResult(versionInfo->GetVersion(&major, &minor));
            return {major, minor};
        }

        DxcOperationResult Validate(const DxcBlob& blob) const {
            if (!m_validator) {
                throw HassleError("Invalid validator");
            }
            Microsoft::WRL::ComPtr<IDxcOperationResult> result;
            CheckHResult(m_validator->Validate(
                blob.GetRaw(),
                DXC_VALIDATOR_FLAGS_IN_PLACE_EDIT,
                &result));
            return DxcOperationResult(result.Get());
        }

        IDxcValidator* GetRaw() const { return m_validator.Get(); }
        bool IsValid() const { return m_validator != nullptr; }
    };

    DxcValidator CreateValidator() const {
        auto createInstance = GetDxcCreateInstance();
        Microsoft::WRL::ComPtr<IDxcValidator> validator;
        CheckHResult(createInstance(
            CLSID_DxcValidator,
            __uuidof(IDxcValidator),
            &validator));
        return DxcValidator(validator.Get());
    }
};

} // namespace hassle
