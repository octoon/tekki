#pragma once

#include "hassle/os.h"
#include <cstdint>
#include <unknwn.h>

// https://learn.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
#define HASSLE_CP_ACP 0
#define HASSLE_CP_UTF8 65001
#define HASSLE_DFCC_DXIL 0x4C495844

namespace hassle {
struct IDxcBlob;
struct IDxcBlobEncoding;
struct IDxcLibrary;
struct IDxcOperationResult;
struct IDxcIncludeHandler;
struct IDxcCompiler;
struct IDxcCompiler2;
struct IDxcLinker;
struct IDxcValidator;
struct IDxcContainerBuilder;
struct IDxcAssembler;
struct IDxcContainerReflection;
struct ID3D12ShaderReflection;
struct IDxcOptimizerPass;
struct IDxcOptimizer;
struct IDxcVersionInfo;
struct IDxcVersionInfo2;

// DxcDefine structure
struct DxcDefine {
    LPCWSTR Name;
    LPCWSTR Value;
};

// IDxcBlob interface
// UUID: 8ba5fb08-5195-40e2-ac58-0d989c3a0102
struct DECLSPEC_UUID("8ba5fb08-5195-40e2-ac58-0d989c3a0102") DECLSPEC_NOVTABLE
IDxcBlob : public IUnknown {
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};

// IDxcBlobEncoding interface
// UUID: 7241d424-2646-4191-97c0-98e96e42fc68
struct DECLSPEC_UUID("7241d424-2646-4191-97c0-98e96e42fc68") DECLSPEC_NOVTABLE
IDxcBlobEncoding : public IDxcBlob {
    virtual HRESULT STDMETHODCALLTYPE GetEncoding(
        _Out_ BOOL* pKnown,
        _Out_ uint32_t* pCodePage) = 0;
};

// IDxcLibrary interface
// UUID: e5204dc7-d18c-4c3c-bdfb-851673980fe7
struct DECLSPEC_UUID("e5204dc7-d18c-4c3c-bdfb-851673980fe7") DECLSPEC_NOVTABLE
IDxcLibrary : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetMalloc(_In_opt_ IMalloc* pMalloc) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
        _In_ IDxcBlob* pBlob,
        _In_ uint32_t offset,
        _In_ uint32_t length,
        _COM_Outptr_ IDxcBlob** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBlobFromFile(
        _In_ LPCWSTR pFileName,
        _In_opt_ uint32_t* pCodePage,
        _COM_Outptr_ IDxcBlobEncoding** ppBlobEncoding) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingFromPinned(
        _In_reads_bytes_(size) LPCVOID pText,
        _In_ uint32_t size,
        _In_ uint32_t codePage,
        _COM_Outptr_ IDxcBlobEncoding** ppBlobEncoding) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnHeapCopy(
        _In_reads_bytes_(size) LPCVOID pText,
        _In_ uint32_t size,
        _In_ uint32_t codePage,
        _COM_Outptr_ IDxcBlobEncoding** ppBlobEncoding) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnMalloc(
        _In_reads_bytes_(size) LPCVOID pText,
        _In_ IMalloc* pIMalloc,
        _In_ uint32_t size,
        _In_ uint32_t codePage,
        _COM_Outptr_ IDxcBlobEncoding** ppBlobEncoding) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateIncludeHandler(
        _COM_Outptr_ IDxcIncludeHandler** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateStreamFromBlobReadOnly(
        _In_ IDxcBlob* pBlob,
        _COM_Outptr_ IStream** ppStream) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
        _In_ IDxcBlob* pBlob,
        _COM_Outptr_ IDxcBlobEncoding** ppBlobEncoding) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
        _In_ IDxcBlob* pBlob,
        _COM_Outptr_ IDxcBlobEncoding** ppBlobEncoding) = 0;
};

// IDxcOperationResult interface
// UUID: cedb484a-d4e9-445a-b991-ca21ca157dc2
struct DECLSPEC_UUID("cedb484a-d4e9-445a-b991-ca21ca157dc2") DECLSPEC_NOVTABLE
IDxcOperationResult : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetStatus(_Out_ HRESULT* pStatus) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResult(_COM_Outptr_ IDxcBlob** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetErrorBuffer(_COM_Outptr_ IDxcBlobEncoding** ppErrors) = 0;
};

// IDxcIncludeHandler interface
// UUID: 7f61fc7d-950d-467f-b3e3-3c02fb49187c
struct DECLSPEC_UUID("7f61fc7d-950d-467f-b3e3-3c02fb49187c") DECLSPEC_NOVTABLE
IDxcIncludeHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE LoadSource(
        _In_ LPCWSTR pFilename,
        _COM_Outptr_ IDxcBlob** ppIncludeSource) = 0;
};

// IDxcCompiler interface
// UUID: 8c210bf3-011f-4422-8d70-6f9acb8db617
struct DECLSPEC_UUID("8c210bf3-011f-4422-8d70-6f9acb8db617") DECLSPEC_NOVTABLE
IDxcCompiler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Compile(
        _In_ IDxcBlob* pSource,
        _In_opt_ LPCWSTR pSourceName,
        _In_ LPCWSTR pEntryPoint,
        _In_ LPCWSTR pTargetProfile,
        _In_reads_opt_(argCount) LPCWSTR* pArguments,
        _In_ uint32_t argCount,
        _In_reads_opt_(defineCount) const DxcDefine* pDefines,
        _In_ uint32_t defineCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE Preprocess(
        _In_ IDxcBlob* pSource,
        _In_opt_ LPCWSTR pSourceName,
        _In_reads_opt_(argCount) LPCWSTR* pArguments,
        _In_ uint32_t argCount,
        _In_reads_opt_(defineCount) const DxcDefine* pDefines,
        _In_ uint32_t defineCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE Disassemble(
        _In_ IDxcBlob* pSource,
        _COM_Outptr_ IDxcBlobEncoding** ppDisassembly) = 0;
};

// IDxcCompiler2 interface
// UUID: a005a9d9-b8bb-4594-b5c9-0e633bec4d37
struct DECLSPEC_UUID("a005a9d9-b8bb-4594-b5c9-0e633bec4d37") DECLSPEC_NOVTABLE
IDxcCompiler2 : public IDxcCompiler {
    virtual HRESULT STDMETHODCALLTYPE CompileWithDebug(
        _In_ IDxcBlob* pSource,
        _In_opt_ LPCWSTR pSourceName,
        _In_ LPCWSTR pEntryPoint,
        _In_ LPCWSTR pTargetProfile,
        _In_reads_opt_(argCount) LPCWSTR* pArguments,
        _In_ uint32_t argCount,
        _In_reads_opt_(defineCount) const DxcDefine* pDefines,
        _In_ uint32_t defineCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult** ppResult,
        _Outptr_opt_result_z_ LPWSTR* ppDebugBlobName,
        _COM_Outptr_opt_ IDxcBlob** ppDebugBlob) = 0;
};

// IDxcLinker interface
// UUID: f1b5be2a-62dd-4327-a1c2-42ac1e1e78e6
struct DECLSPEC_UUID("f1b5be2a-62dd-4327-a1c2-42ac1e1e78e6") DECLSPEC_NOVTABLE
IDxcLinker : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE RegisterLibrary(
        _In_opt_ LPCWSTR pLibName,
        _In_ IDxcBlob* pLib) = 0;
    virtual HRESULT STDMETHODCALLTYPE Link(
        _In_ LPCWSTR pEntryName,
        _In_ LPCWSTR pTargetProfile,
        _In_reads_(libCount) LPCWSTR* pLibNames,
        _In_ uint32_t libCount,
        _In_reads_opt_(argCount) LPCWSTR* pArguments,
        _In_ uint32_t argCount,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;
};

// Validator flags
constexpr uint32_t DXC_VALIDATOR_FLAGS_DEFAULT = 0;
constexpr uint32_t DXC_VALIDATOR_FLAGS_IN_PLACE_EDIT = 1;
constexpr uint32_t DXC_VALIDATOR_FLAGS_ROOT_SIGNATURE_ONLY = 2;
constexpr uint32_t DXC_VALIDATOR_FLAGS_MODULE_ONLY = 4;
constexpr uint32_t DXC_VALIDATOR_FLAGS_VALID_MASK = 0x7;

// IDxcValidator interface
// UUID: a6e82bd2-1fd7-4826-9811-2857e797f49a
struct DECLSPEC_UUID("a6e82bd2-1fd7-4826-9811-2857e797f49a") DECLSPEC_NOVTABLE
IDxcValidator : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Validate(
        _In_ IDxcBlob* pShader,
        _In_ uint32_t Flags,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;
};

// IDxcContainerBuilder interface
// UUID: 334b1f50-2292-4b35-99a1-25588d8c17fe
struct DECLSPEC_UUID("334b1f50-2292-4b35-99a1-25588d8c17fe") DECLSPEC_NOVTABLE
IDxcContainerBuilder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob* pDxilContainerHeader) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddPart(_In_ uint32_t fourCC, _In_ IDxcBlob* pSource) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemovePart(_In_ uint32_t fourCC) = 0;
    virtual HRESULT STDMETHODCALLTYPE SerializeContainer(_Out_ IDxcOperationResult** ppResult) = 0;
};

// IDxcAssembler interface
// UUID: 091f7a26-1c1f-4948-904b-e6e3a8a771d5
struct DECLSPEC_UUID("091f7a26-1c1f-4948-904b-e6e3a8a771d5") DECLSPEC_NOVTABLE
IDxcAssembler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE AssembleToContainer(
        _In_ IDxcBlob* pShader,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;
};

// IDxcContainerReflection interface
// UUID: d2c21b26-8350-4bdc-976a-331ce6f4c54c
struct DECLSPEC_UUID("d2c21b26-8350-4bdc-976a-331ce6f4c54c") DECLSPEC_NOVTABLE
IDxcContainerReflection : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob* pContainer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPartCount(_Out_ uint32_t* pResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPartKind(_In_ uint32_t idx, _Out_ uint32_t* pResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPartContent(_In_ uint32_t idx, _COM_Outptr_ IDxcBlob** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindFirstPartKind(_In_ uint32_t kind, _Out_ uint32_t* pResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPartReflection(
        _In_ uint32_t idx,
        _In_ REFIID iid,
        _COM_Outptr_ void** ppvObject) = 0;
};

// ID3D12ShaderReflection interface
// UUID: 5a58797d-a72c-478d-8ba2-efc6b0efe88e
struct DECLSPEC_UUID("5a58797d-a72c-478d-8ba2-efc6b0efe88e") DECLSPEC_NOVTABLE
ID3D12ShaderReflection : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDesc(_Out_ void* pDesc) = 0;
    virtual void* STDMETHODCALLTYPE GetConstantBufferByIndex(_In_ uint32_t Index) = 0;
    virtual void* STDMETHODCALLTYPE GetConstantBufferByName(_In_ LPCSTR Name) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResourceBindingDesc(
        _In_ uint32_t ResourceIndex,
        _Out_ void* pDesc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInputParameterDesc(
        _In_ uint32_t ParameterIndex,
        _Out_ void* pDesc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOutputParameterDesc(
        _In_ uint32_t ParameterIndex,
        _Out_ void* pDesc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPatchConstantParameterDesc(
        _In_ uint32_t ParameterIndex,
        _Out_ void* pDesc) = 0;
    virtual void* STDMETHODCALLTYPE GetVariableByName(_In_ LPCSTR Name) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResourceBindingDescByName(
        _In_ LPCSTR Name,
        _Out_ void* pDesc) = 0;
    virtual uint32_t STDMETHODCALLTYPE GetMovInstructionCount() = 0;
    virtual uint32_t STDMETHODCALLTYPE GetMovcInstructionCount() = 0;
    virtual uint32_t STDMETHODCALLTYPE GetConversionInstructionCount() = 0;
    virtual uint32_t STDMETHODCALLTYPE GetBitwiseInstructionCount() = 0;
    virtual uint32_t STDMETHODCALLTYPE GetGSInputPrimitive() = 0;
    virtual BOOL STDMETHODCALLTYPE IsSampleFrequencyShader() = 0;
    virtual uint32_t STDMETHODCALLTYPE GetNumInterfaceSlots() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMinFeatureLevel(_Out_ void* pLevel) = 0;
    virtual uint32_t STDMETHODCALLTYPE GetThreadGroupSize(
        _Out_opt_ uint32_t* pSizeX,
        _Out_opt_ uint32_t* pSizeY,
        _Out_opt_ uint32_t* pSizeZ) = 0;
    virtual uint64_t STDMETHODCALLTYPE GetRequiresFlags() = 0;
};

// IDxcOptimizerPass interface
// UUID: ae2cd79f-cc22-453f-9b6b-b124e7a5204c
struct DECLSPEC_UUID("ae2cd79f-cc22-453f-9b6b-b124e7a5204c") DECLSPEC_NOVTABLE
IDxcOptimizerPass : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetOptionName(_Outptr_ LPWSTR* ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDescription(_Outptr_ LPWSTR* ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOptionArgCount(_Out_ uint32_t* pCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOptionArgName(_In_ uint32_t argIndex, _Outptr_ LPWSTR* ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOptionArgDescription(_In_ uint32_t argIndex, _Outptr_ LPWSTR* ppResult) = 0;
};

// IDxcOptimizer interface
// UUID: 25740e2e-9cba-401b-9119-4fb42f39f270
struct DECLSPEC_UUID("25740e2e-9cba-401b-9119-4fb42f39f270") DECLSPEC_NOVTABLE
IDxcOptimizer : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetAvailablePassCount(_Out_ uint32_t* pCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAvailablePass(_In_ uint32_t index, _COM_Outptr_ IDxcOptimizerPass** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE RunOptimizer(
        _In_ IDxcBlob* pBlob,
        _In_count_(optionCount) LPCWSTR* ppOptions,
        _In_ uint32_t optionCount,
        _COM_Outptr_ IDxcBlob** ppOutputModule,
        _COM_Outptr_opt_ IDxcBlobEncoding** ppOutputText) = 0;
};

// Version info flags
constexpr uint32_t DXC_VERSION_INFO_FLAGS_NONE = 0;
constexpr uint32_t DXC_VERSION_INFO_FLAGS_DEBUG = 1;     // Matches VS_FF_DEBUG
constexpr uint32_t DXC_VERSION_INFO_FLAGS_INTERNAL = 2; // Internal Validator (non-signing)

// IDxcVersionInfo interface
// UUID: b04f5b50-2059-4f12-a8ff-a1e0cde1cc7e
struct DECLSPEC_UUID("b04f5b50-2059-4f12-a8ff-a1e0cde1cc7e") DECLSPEC_NOVTABLE
IDxcVersionInfo : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetVersion(_Out_ uint32_t* pMajor, _Out_ uint32_t* pMinor) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFlags(_Out_ uint32_t* pFlags) = 0;
};

// IDxcVersionInfo2 interface
// UUID: fb6904c4-42f0-4b62-9c46-983af7da7c83
struct DECLSPEC_UUID("fb6904c4-42f0-4b62-9c46-983af7da7c83") DECLSPEC_NOVTABLE
IDxcVersionInfo2 : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCommitInfo(
        _Out_ uint32_t* pCommitCount,
        _Outptr_ char** pCommitHash) = 0;
};

// CLSID definitions
static constexpr GUID CLSID_DxcCompiler = {
    0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};
static constexpr GUID CLSID_DxcLinker = {
    0xef6a8087, 0xb0ea, 0x4d56, {0x9e, 0x45, 0xd0, 0x7e, 0x1a, 0x8b, 0x78, 0x06}};
static constexpr GUID CLSID_DxcDiaDataSource = {
    0xcd1f6b73, 0x2ab0, 0x484d, {0x8e, 0xdc, 0xeb, 0xe7, 0xa4, 0x3c, 0xa0, 0x9f}};
static constexpr GUID CLSID_DxcLibrary = {
    0x6245d6af, 0x66e0, 0x48fd, {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c}};
static constexpr GUID CLSID_DxcValidator = {
    0x8ca3e215, 0xf728, 0x4cf3, {0x8c, 0xdd, 0x88, 0xaf, 0x91, 0x75, 0x87, 0xa1}};
static constexpr GUID CLSID_DxcAssembler = {
    0xd728db68, 0xf903, 0x4f80, {0x94, 0xcd, 0xdc, 0xcf, 0x76, 0xec, 0x71, 0x51}};
static constexpr GUID CLSID_DxcContainerReflection = {
    0xb9f54489, 0x55b8, 0x400c, {0xba, 0x3a, 0x16, 0x75, 0xe4, 0x72, 0x8b, 0x91}};
static constexpr GUID CLSID_DxcOptimizer = {
    0xae2cd79f, 0xcc22, 0x453f, {0x9b, 0x6b, 0xb1, 0x24, 0xe7, 0xa5, 0x20, 0x4c}};
static constexpr GUID CLSID_DxcContainerBuilder = {
    0x94134294, 0x411f, 0x4574, {0xb4, 0xd0, 0x87, 0x41, 0xe2, 0x52, 0x40, 0xd2}};

// Function pointer type for DxcCreateInstance
using DxcCreateInstanceProc = HRESULT(WINAPI*)(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _COM_Outptr_ LPVOID* ppv);

using DxcCreateInstanceProc2 = HRESULT(WINAPI*)(
    _In_opt_ IMalloc* pMalloc,
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _COM_Outptr_ LPVOID* ppv);

} // namespace hassle
