#include "hassle/utils.h"
#include "hassle/wrapper.h"
#include "hassle/fake_sign/fake_sign.h"
#include <codecvt>
#include <locale>
#include <cstring>

namespace hassle {

std::wstring ToWide(const std::string& str) {
    if (str.empty()) {
        return L"";
    }

#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
#endif
}

std::string FromWide(const wchar_t* wide) {
    if (wide == nullptr) {
        return "";
    }

#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide);
#endif
}

std::string FromBstr(BSTR bstr) {
    if (bstr == nullptr) {
        return "";
    }

#ifdef _WIN32
    uint32_t len = SysStringLen(bstr);
    std::wstring wstr(bstr, len);
    std::string result = FromWide(wstr.c_str());
    SysFreeString(bstr);
    return result;
#else
    uint32_t len = os::SysStringLen(bstr);
    std::wstring wstr(bstr, len);
    std::string result = FromWide(wstr.c_str());
    os::SysFreeString(bstr);
    return result;
#endif
}

std::string FromLpstr(LPCSTR lpstr) {
    if (lpstr == nullptr) {
        return "";
    }
    return std::string(lpstr);
}

// Default include handler implementation
class DefaultIncludeHandler : public DxcIncludeHandlerInterface {
public:
    std::optional<std::string> LoadSource(const std::string& filename) override {
        FILE* file = fopen(filename.c_str(), "rb");
        if (!file) {
            return std::nullopt;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::string content(size, '\0');
        fread(&content[0], 1, size, file);
        fclose(file);

        return content;
    }
};

OperationOutput OperationOutput::FromOperationResult(const DxcOperationResult& result) {
    // Get error buffer
    auto error = result.GetErrorBuffer();
    std::string errorStr;
    if (auto str = error.AsStr()) {
        errorStr = *str;
    }

    // Get result blob
    auto output = result.GetResult();

    // Get status
    HResult status = result.GetStatus();

    OperationOutput out;

    if (status.IsError()) {
        // On error, output should be empty
        throw OperationError(status, errorStr);
    } else {
        // On success, we might still have warnings
        if (!errorStr.empty()) {
            out.messages = errorStr;
        }
        out.blob = output.ToVec<uint8_t>();
    }

    return out;
}

OperationOutput CompileHlsl(
    const std::string& sourceName,
    const std::string& shaderText,
    const std::string& entryPoint,
    const std::string& targetProfile,
    const std::vector<std::string>& args,
    const std::vector<std::pair<std::string, std::optional<std::string>>>& defines)
{
    Dxc dxc;

    auto compiler = dxc.CreateCompiler();
    auto library = dxc.CreateLibrary();

    auto blob = library.CreateBlobWithEncodingFromStr(shaderText);

    DefaultIncludeHandler includeHandler;
    auto result = compiler.Compile(
        blob,
        sourceName,
        entryPoint,
        targetProfile,
        args,
        &includeHandler,
        defines);

    return OperationOutput::FromOperationResult(result);
}

OperationOutput ValidateDxil(const std::vector<uint8_t>& data) {
    Dxc dxc;
    Dxil dxil;

    auto validator = dxil.CreateValidator();
    auto library = dxc.CreateLibrary();

    auto blobEncoding = library.CreateBlobWithEncoding(data);
    auto result = validator.Validate(blobEncoding);

    return OperationOutput::FromOperationResult(result);
}

bool FakeSignDxilInPlace(std::vector<uint8_t>& dxil) {
    return fake_sign::FakeSignDxilInPlace(dxil);
}

} // namespace hassle
