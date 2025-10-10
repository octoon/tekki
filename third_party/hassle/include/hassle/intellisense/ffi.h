#pragma once

#include "hassle/os.h"
#include "hassle/ffi.h"

namespace hassle {
namespace intellisense {

// Enumerations and flags

enum class DxcGlobalOptions : uint32_t {
    None = 0x0,
    ThreadBackgroundPriorityForIndexing = 0x1,
    ThreadBackgroundPriorityForEditing = 0x2,
    ThreadBackgroundPriorityForAll = 0x3,
};

enum class DxcDiagnosticSeverity : uint32_t {
    Ignored = 0,
    Note = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4,
};

enum class DxcTokenKind : uint32_t {
    Punctuation = 0,
    Keyword = 1,
    Identifier = 2,
    Literal = 3,
    Comment = 4,
    Unknown = 5,
    BuiltInType = 6,
};

enum class DxcTypeKind : uint32_t {
    Invalid = 0,
    Unexposed = 1,
    Void = 2,
    Bool = 3,
    Char_U = 4,
    UChar = 5,
    Char16 = 6,
    Char32 = 7,
    UShort = 8,
    UInt = 9,
    ULong = 10,
    ULongLong = 11,
    UInt128 = 12,
    Char_S = 13,
    SChar = 14,
    WChar = 15,
    Short = 16,
    Int = 17,
    Long = 18,
    LongLong = 19,
    Int128 = 20,
    Float = 21,
    Double = 22,
    LongDouble = 23,
    NullPtr = 24,
    Overload = 25,
    Dependent = 26,
    ObjCId = 27,
    ObjCClass = 28,
    ObjCSel = 29,
    FirstBuiltin = 2,
    LastBuiltin = 29,
    Complex = 100,
    Pointer = 101,
    BlockPointer = 102,
    LValueReference = 103,
    RValueReference = 104,
    Record = 105,
    Enum = 106,
    Typedef = 107,
    ObjCInterface = 108,
    ObjCObjectPointer = 109,
    FunctionNoProto = 110,
    FunctionProto = 111,
    ConstantArray = 112,
    Vector = 113,
    IncompleteArray = 114,
    VariableArray = 115,
    DependentSizedArray = 116,
    MemberPointer = 117,
};

enum class DxcCursorFormatting : uint32_t {
    Default = 0x0,
    UseLanguageOptions = 0x1,
    SuppressSpecifiers = 0x2,
    SuppressTagKeyword = 0x4,
    IncludeNamespaceKeyword = 0x8,
};

enum class DxcTranslationUnitFlags : uint32_t {
    None = 0x0,
    DetailedPreprocessingRecord = 0x01,
    Incomplete = 0x02,
    PrecompiledPreamble = 0x04,
    CacheCompletionResults = 0x08,
    ForSerialization = 0x10,
    CXXChainedPCH = 0x20,
    SkipFunctionBodies = 0x40,
    IncludeBriefCommentsInCodeCompletion = 0x80,
    UseCallerThread = 0x800,
};

enum class DxcDiagnosticDisplayOptions : uint32_t {
    DisplaySourceLocation = 0x01,
    DisplayColumn = 0x02,
    DisplaySourceRanges = 0x04,
    DisplayOption = 0x08,
    DisplayCategoryID = 0x10,
    DisplayCategoryName = 0x20,
    DisplaySeverity = 0x200,
};

enum class DxcCursorKindFlags : uint32_t {
    None = 0,
    Declaration = 0x1,
    Reference = 0x2,
    Expression = 0x4,
    Statement = 0x8,
    Attribute = 0x10,
    Invalid = 0x20,
    TranslationUnit = 0x40,
    Preprocessing = 0x80,
    Unexposed = 0x100,
};

enum class DxcCursorKind : uint32_t {
    UnexposedDecl = 1,
    StructDecl = 2,
    UnionDecl = 3,
    ClassDecl = 4,
    EnumDecl = 5,
    FieldDecl = 6,
    EnumConstantDecl = 7,
    FunctionDecl = 8,
    VarDecl = 9,
    ParmDecl = 10,
    ObjCInterfaceDecl = 11,
    ObjCCategoryDecl = 12,
    ObjCProtocolDecl = 13,
    ObjCPropertyDecl = 14,
    ObjCIvarDecl = 15,
    ObjCInstanceMethodDecl = 16,
    ObjCClassMethodDecl = 17,
    ObjCImplementationDecl = 18,
    ObjCCategoryImplDecl = 19,
    TypedefDecl = 20,
    CXXMethod = 21,
    Namespace = 22,
    LinkageSpec = 23,
    Constructor = 24,
    Destructor = 25,
    ConversionFunction = 26,
    TemplateTypeParameter = 27,
    NonTypeTemplateParameter = 28,
    TemplateTemplateParameter = 29,
    FunctionTemplate = 30,
    ClassTemplate = 31,
    ClassTemplatePartialSpecialization = 32,
    NamespaceAlias = 33,
    UsingDirective = 34,
    UsingDeclaration = 35,
    TypeAliasDecl = 36,
    ObjCSynthesizeDecl = 37,
    ObjCDynamicDecl = 38,
    CXXAccessSpecifier = 39,
    FirstDecl = 1,
    LastDecl = 39,
    FirstRef = 40,
    ObjCSuperClassRef = 40,
    ObjCProtocolRef = 41,
    ObjCClassRef = 42,
    TypeRef = 43,
    CXXBaseSpecifier = 44,
    TemplateRef = 45,
    NamespaceRef = 46,
    MemberRef = 47,
    LabelRef = 48,
    OverloadedDeclRef = 49,
    VariableRef = 50,
    LastRef = 50,
    FirstInvalid = 70,
    InvalidFile = 70,
    NoDeclFound = 71,
    NotImplemented = 72,
    InvalidCode = 73,
    LastInvalid = 73,
    FirstExpr = 100,
    UnexposedExpr = 100,
    DeclRefExpr = 101,
    MemberRefExpr = 102,
    CallExpr = 103,
    ObjCMessageExpr = 104,
    BlockExpr = 105,
    IntegerLiteral = 106,
    FloatingLiteral = 107,
    ImaginaryLiteral = 108,
    StringLiteral = 109,
    CharacterLiteral = 110,
    ParenExpr = 111,
    UnaryOperator = 112,
    ArraySubscriptExpr = 113,
    BinaryOperator = 114,
    CompoundAssignOperator = 115,
    ConditionalOperator = 116,
    CStyleCastExpr = 117,
    CompoundLiteralExpr = 118,
    InitListExpr = 119,
    AddrLabelExpr = 120,
    StmtExpr = 121,
    GenericSelectionExpr = 122,
    GNUNullExpr = 123,
    CXXStaticCastExpr = 124,
    CXXDynamicCastExpr = 125,
    CXXReinterpretCastExpr = 126,
    CXXConstCastExpr = 127,
    CXXFunctionalCastExpr = 128,
    CXXTypeidExpr = 129,
    CXXBoolLiteralExpr = 130,
    CXXNullPtrLiteralExpr = 131,
    CXXThisExpr = 132,
    CXXThrowExpr = 133,
    CXXNewExpr = 134,
    CXXDeleteExpr = 135,
    UnaryExpr = 136,
    ObjCStringLiteral = 137,
    ObjCEncodeExpr = 138,
    ObjCSelectorExpr = 139,
    ObjCProtocolExpr = 140,
    ObjCBridgedCastExpr = 141,
    PackExpansionExpr = 142,
    SizeOfPackExpr = 143,
    LambdaExpr = 144,
    ObjCBoolLiteralExpr = 145,
    ObjCSelfExpr = 146,
    LastExpr = 146,
    FirstStmt = 200,
    UnexposedStmt = 200,
    LabelStmt = 201,
    CompoundStmt = 202,
    CaseStmt = 203,
    DefaultStmt = 204,
    IfStmt = 205,
    SwitchStmt = 206,
    WhileStmt = 207,
    DoStmt = 208,
    ForStmt = 209,
    GotoStmt = 210,
    IndirectGotoStmt = 211,
    ContinueStmt = 212,
    BreakStmt = 213,
    ReturnStmt = 214,
    GCCAsmStmt = 215,
    AsmStmt = 215,
    ObjCAtTryStmt = 216,
    ObjCAtCatchStmt = 217,
    ObjCAtFinallyStmt = 218,
    ObjCAtThrowStmt = 219,
    ObjCAtSynchronizedStmt = 220,
    ObjCAutoreleasePoolStmt = 221,
    ObjCForCollectionStmt = 222,
    CXXCatchStmt = 223,
    CXXTryStmt = 224,
    CXXForRangeStmt = 225,
    SEHTryStmt = 226,
    SEHExceptStmt = 227,
    SEHFinallyStmt = 228,
    MSAsmStmt = 229,
    NullStmt = 230,
    DeclStmt = 231,
    OMPParallelDirective = 232,
    OMPSimdDirective = 233,
    OMPForDirective = 234,
    OMPSectionsDirective = 235,
    OMPSectionDirective = 236,
    OMPSingleDirective = 237,
    OMPParallelForDirective = 238,
    OMPParallelSectionsDirective = 239,
    OMPTaskDirective = 240,
    OMPMasterDirective = 241,
    OMPCriticalDirective = 242,
    OMPTaskyieldDirective = 243,
    OMPBarrierDirective = 244,
    OMPTaskwaitDirective = 245,
    OMPFlushDirective = 246,
    SEHLeaveStmt = 247,
    OMPOrderedDirective = 248,
    OMPAtomicDirective = 249,
    OMPForSimdDirective = 250,
    OMPParallelForSimdDirective = 251,
    OMPTargetDirective = 252,
    OMPTeamsDirective = 253,
    OMPTaskgroupDirective = 254,
    OMPCancellationPointDirective = 255,
    OMPCancelDirective = 256,
    LastStmt = 256,
    TranslationUnit = 300,
    FirstAttr = 400,
    UnexposedAttr = 400,
    IBActionAttr = 401,
    IBOutletAttr = 402,
    IBOutletCollectionAttr = 403,
    CXXFinalAttr = 404,
    CXXOverrideAttr = 405,
    AnnotateAttr = 406,
    AsmLabelAttr = 407,
    PackedAttr = 408,
    PureAttr = 409,
    ConstAttr = 410,
    NoDuplicateAttr = 411,
    CUDAConstantAttr = 412,
    CUDADeviceAttr = 413,
    CUDAGlobalAttr = 414,
    CUDAHostAttr = 415,
    CUDASharedAttr = 416,
    LastAttr = 416,
    PreprocessingDirective = 500,
    MacroDefinition = 501,
    MacroExpansion = 502,
    MacroInstantiation = 502,
    InclusionDirective = 503,
    FirstPreprocessing = 500,
    LastPreprocessing = 503,
    ModuleImportDecl = 600,
    FirstExtraDecl = 600,
    LastExtraDecl = 600,
};

// COM Interface forward declarations
struct IDxcDiagnostic;
struct IDxcInclusion;
struct IDxcToken;
struct IDxcType;
struct IDxcSourceLocation;
struct IDxcSourceRange;
struct IDxcCursor;
struct IDxcUnsavedFile;
struct IDxcFile;
struct IDxcTranslationUnit;
struct IDxcIndex;
struct IDxcIntelliSense;

// COM Interfaces
MIDL_INTERFACE("4f76b234-3659-4d33-99b0-3b0db994b564")
IDxcDiagnostic : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE FormatDiagnostic(
        DxcDiagnosticDisplayOptions options,
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSeverity(
        _Out_ DxcDiagnosticSeverity* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLocation(
        _COM_Outptr_ IDxcSourceLocation** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpelling(
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCategoryText(
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNumRanges(
        _Out_ uint32_t* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRangeAt(
        uint32_t index,
        _COM_Outptr_ IDxcSourceRange** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNumFixIts(
        _Out_ uint32_t* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFixItAt(
        uint32_t index,
        _COM_Outptr_ IDxcSourceRange** replacementRange,
        _Outptr_result_z_ LPSTR* text) = 0;
};

MIDL_INTERFACE("0c364d65-df44-4412-888e-4e552fc5e3d6")
IDxcInclusion : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetIncludedFile(
        _COM_Outptr_ IDxcFile** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStackLength(
        _Out_ uint32_t* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStackItem(
        uint32_t index,
        _COM_Outptr_ IDxcSourceLocation** result) = 0;
};

MIDL_INTERFACE("7f90b9ff-a275-4932-97d8-3cfd234482a2")
IDxcToken : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetKind(
        _Out_ DxcTokenKind* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLocation(
        _COM_Outptr_ IDxcSourceLocation** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExtent(
        _COM_Outptr_ IDxcSourceRange** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpelling(
        _Outptr_result_z_ LPSTR* value) = 0;
};

MIDL_INTERFACE("2ec912fd-b144-4a15-ad0d-1c5439c81e46")
IDxcType : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetSpelling(
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsEqualTo(
        _In_ IDxcType* other,
        _Out_ bool* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetKind(
        _Out_ DxcCursorKind* result) = 0;
};

MIDL_INTERFACE("8e7ddf1c-d7d3-4d69-b286-85fccba1e0cf")
IDxcSourceLocation : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsEqualTo(
        _In_ IDxcSourceLocation* other,
        _Out_ bool* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpellingLocation(
        _COM_Outptr_opt_ IDxcFile** file,
        _Out_ uint32_t* line,
        _Out_ uint32_t* col,
        _Out_ uint32_t* offset) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsNull(
        _Out_ bool* result) = 0;
};

MIDL_INTERFACE("f1359b36-a53f-4e81-b514-b6b84122a13f")
IDxcSourceRange : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsNull(
        _Out_ bool* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStart(
        _COM_Outptr_ IDxcSourceLocation** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEnd(
        _COM_Outptr_ IDxcSourceLocation** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOffsets(
        _Out_ uint32_t* startOffset,
        _Out_ uint32_t* endOffset) = 0;
};

MIDL_INTERFACE("1467b985-288d-4d2a-80c1-ef89c42c40bc")
IDxcCursor : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetExtent(
        _COM_Outptr_ IDxcSourceRange** range) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLocation(
        _COM_Outptr_ IDxcSourceLocation** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetKind(
        _Out_ DxcCursorKind* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetKindFlags(
        _Out_ DxcCursorKindFlags* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSemanticParent(
        _COM_Outptr_ IDxcCursor** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLexicalParent(
        _COM_Outptr_ IDxcCursor** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCursorType(
        _COM_Outptr_ IDxcType** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNumArguments(
        _Out_ int32_t* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetArgumentAt(
        int32_t index,
        _COM_Outptr_ IDxcCursor** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetReferencedCursor(
        _COM_Outptr_ IDxcCursor** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDefinitionCursor(
        _COM_Outptr_ IDxcCursor** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindReferencesInFile(
        _In_ IDxcFile* file,
        uint32_t skip,
        uint32_t top,
        _Out_ uint32_t* resultLength,
        _Outptr_result_buffer_(*resultLength) IDxcCursor*** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpelling(
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsEqualTo(
        _In_ IDxcCursor* other,
        _Out_ bool* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsNull(
        _Out_ bool* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsDefinition(
        _Out_ bool* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplayName(
        _Out_ BSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetQualifiedName(
        bool includeTemplateArgs,
        _Out_ BSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFormattedName(
        DxcCursorFormatting formatting,
        _Out_ BSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChildren(
        uint32_t skip,
        uint32_t top,
        _Out_ uint32_t* resultLength,
        _Outptr_result_buffer_(*resultLength) IDxcCursor*** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSnappedChild(
        _In_ IDxcSourceLocation* location,
        _COM_Outptr_ IDxcCursor** result) = 0;
};

MIDL_INTERFACE("8ec00f98-07d0-4e60-9d7c-5a50b5b0017f")
IDxcUnsavedFile : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetFileName(
        _Outptr_result_z_ LPSTR* fileName) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetContents(
        _Outptr_result_z_ LPSTR* contents) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLength(
        _Out_ uint32_t* length) = 0;
};

MIDL_INTERFACE("bb2fca9e-1478-47ba-b08c-2c502ada4895")
IDxcFile : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetName(
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsEqualTo(
        _In_ IDxcFile* other,
        _Out_ bool* result) = 0;
};

MIDL_INTERFACE("9677dee0-c0e5-46a1-8b40-3db3168be63d")
IDxcTranslationUnit : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCursor(
        _COM_Outptr_ IDxcCursor** cursor) = 0;
    virtual HRESULT STDMETHODCALLTYPE Tokenize(
        _In_ IDxcSourceRange* range,
        _Outptr_result_buffer_(*tokenCount) IDxcToken*** tokens,
        _Out_ uint32_t* tokenCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLocation(
        _In_ IDxcFile* file,
        uint32_t line,
        uint32_t column,
        _COM_Outptr_ IDxcSourceLocation** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNumDiagnostics(
        _Out_ uint32_t* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDiagnostic(
        uint32_t index,
        _COM_Outptr_ IDxcDiagnostic** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFile(
        _In_z_ const char* name,
        _COM_Outptr_ IDxcFile** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFileName(
        _Outptr_result_z_ LPSTR* result) = 0;
    virtual HRESULT STDMETHODCALLTYPE Reparse(
        _In_reads_(numUnsavedFiles) IDxcUnsavedFile** unsavedFiles,
        uint32_t numUnsavedFiles) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCursorForLocation(
        _In_ IDxcSourceLocation* location,
        _COM_Outptr_ IDxcCursor** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLocationForOffset(
        _In_ IDxcFile* file,
        uint32_t offset,
        _COM_Outptr_ IDxcSourceLocation** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSkippedRanges(
        _In_ IDxcFile* file,
        _Out_ uint32_t* resultCount,
        _Outptr_result_buffer_(*resultCount) IDxcSourceRange*** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDiagnosticDetails(
        uint32_t index,
        DxcDiagnosticDisplayOptions options,
        _Out_ uint32_t* errorCode,
        _Out_ uint32_t* errorLine,
        _Out_ uint32_t* errorColumn,
        _Out_ BSTR* errorFile,
        _Out_ uint32_t* errorOffset,
        _Out_ uint32_t* errorLength,
        _Out_ BSTR* errorMessage) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInclusionList(
        _Out_ uint32_t* resultCount,
        _Outptr_result_buffer_(*resultCount) IDxcInclusion*** result) = 0;
};

MIDL_INTERFACE("937824a0-7f5a-4815-9b0a-7cc0424f4173")
IDxcIndex : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetGlobalOptions(
        DxcGlobalOptions options) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGlobalOptions(
        _Out_ DxcGlobalOptions* options) = 0;
    virtual HRESULT STDMETHODCALLTYPE ParseTranslationUnit(
        _In_z_ const char* sourceFilename,
        _In_reads_(numCommandLineArgs) const char** commandLineArgs,
        int32_t numCommandLineArgs,
        _In_reads_(numUnsavedFiles) IDxcUnsavedFile** unsavedFiles,
        uint32_t numUnsavedFiles,
        DxcTranslationUnitFlags options,
        _COM_Outptr_ IDxcTranslationUnit** translationUnit) = 0;
};

MIDL_INTERFACE("b1f99513-46d6-4112-8169-dd0d6053f17d")
IDxcIntelliSense : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateIndex(
        _COM_Outptr_ IDxcIndex** index) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNullLocation(
        _COM_Outptr_ IDxcSourceLocation** location) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNullRange(
        _COM_Outptr_ IDxcSourceRange** location) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRange(
        _In_ IDxcSourceLocation* start,
        _In_ IDxcSourceLocation* end,
        _COM_Outptr_ IDxcSourceRange** location) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDefaultDiagnosticDisplayOptions(
        _Out_ DxcDiagnosticDisplayOptions* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDefaultEditingTUOptions(
        _Out_ DxcTranslationUnitFlags* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateUnsavedFile(
        _In_z_ LPCSTR fileName,
        _In_z_ LPCSTR contents,
        uint32_t contentLength,
        _COM_Outptr_ IDxcUnsavedFile** result) = 0;
};

// CLSID for IDxcIntelliSense
static const GUID CLSID_DxcIntelliSense = {
    0x3047833c, 0xd1c0, 0x4b8e,
    {0x9d, 0x40, 0x10, 0x28, 0x78, 0x60, 0x59, 0x85}
};

} // namespace intellisense
} // namespace hassle
