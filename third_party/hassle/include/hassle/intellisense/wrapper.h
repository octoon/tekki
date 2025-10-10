#pragma once

#include "hassle/intellisense/ffi.h"
#include "hassle/utils.h"
#include <wrl/client.h>
#include <vector>
#include <string>

namespace hassle {

// Forward declaration
class Dxc;

namespace intellisense {

// Forward declarations
class DxcType;
class DxcSourceLocation;
class DxcSourceRange;
class DxcFile;
class DxcCursor;
class DxcUnsavedFile;
class DxcTranslationUnit;
class DxcIndex;
class DxcIntelliSense;

// DxcType wrapper
class DxcType {
private:
    Microsoft::WRL::ComPtr<IDxcType> m_type;

public:
    DxcType() = default;
    explicit DxcType(IDxcType* type) : m_type(type) {}

    std::string GetSpelling() const {
        if (!m_type) {
            throw HassleError("Invalid type");
        }
        LPSTR spelling = nullptr;
        CheckHResult(m_type->GetSpelling(&spelling));
        return FromLpstr(spelling);
    }

    IDxcType* GetRaw() const { return m_type.Get(); }
    bool IsValid() const { return m_type != nullptr; }
};

// DxcSourceLocation wrapper
class DxcSourceLocation {
private:
    Microsoft::WRL::ComPtr<IDxcSourceLocation> m_location;

public:
    DxcSourceLocation() = default;
    explicit DxcSourceLocation(IDxcSourceLocation* location) : m_location(location) {}

    IDxcSourceLocation* GetRaw() const { return m_location.Get(); }
    bool IsValid() const { return m_location != nullptr; }
};

// DxcSourceRange wrapper
struct DxcSourceOffsets {
    uint32_t startOffset;
    uint32_t endOffset;
};

class DxcSourceRange {
private:
    Microsoft::WRL::ComPtr<IDxcSourceRange> m_range;

public:
    DxcSourceRange() = default;
    explicit DxcSourceRange(IDxcSourceRange* range) : m_range(range) {}

    DxcSourceOffsets GetOffsets() const {
        if (!m_range) {
            throw HassleError("Invalid source range");
        }
        uint32_t startOffset = 0, endOffset = 0;
        CheckHResult(m_range->GetOffsets(&startOffset, &endOffset));
        return DxcSourceOffsets{startOffset, endOffset};
    }

    IDxcSourceRange* GetRaw() const { return m_range.Get(); }
    bool IsValid() const { return m_range != nullptr; }
};

// DxcFile wrapper
class DxcFile {
private:
    Microsoft::WRL::ComPtr<IDxcFile> m_file;

    friend class DxcTranslationUnit;
    friend class DxcCursor;

public:
    DxcFile() = default;
    explicit DxcFile(IDxcFile* file) : m_file(file) {}

    IDxcFile* GetRaw() const { return m_file.Get(); }
    bool IsValid() const { return m_file != nullptr; }
};

// DxcCursor wrapper
class DxcCursor {
private:
    Microsoft::WRL::ComPtr<IDxcCursor> m_cursor;

public:
    DxcCursor() = default;
    explicit DxcCursor(IDxcCursor* cursor) : m_cursor(cursor) {}

    std::vector<DxcCursor> GetChildren(uint32_t skip, uint32_t maxCount) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }

        IDxcCursor** result = nullptr;
        uint32_t resultLength = 0;
        CheckHResult(m_cursor->GetChildren(skip, maxCount, &resultLength, &result));

        std::vector<DxcCursor> children;
        children.reserve(resultLength);
        for (uint32_t i = 0; i < resultLength; ++i) {
            children.emplace_back(result[i]);
        }

        // Free the buffer allocated by GetChildren
        CoTaskMemFree(result);

        return children;
    }

    std::vector<DxcCursor> GetAllChildren() const {
        constexpr uint32_t MAX_CHILDREN_PER_CHUNK = 10;
        std::vector<DxcCursor> children;

        while (true) {
            auto chunk = GetChildren(static_cast<uint32_t>(children.size()), MAX_CHILDREN_PER_CHUNK);
            size_t chunkSize = chunk.size();
            children.insert(children.end(), chunk.begin(), chunk.end());
            if (chunkSize < MAX_CHILDREN_PER_CHUNK) {
                break;
            }
        }

        return children;
    }

    DxcSourceRange GetExtent() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcSourceRange> range;
        CheckHResult(m_cursor->GetExtent(&range));
        return DxcSourceRange(range.Get());
    }

    DxcSourceLocation GetLocation() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcSourceLocation> location;
        CheckHResult(m_cursor->GetLocation(&location));
        return DxcSourceLocation(location.Get());
    }

    std::string GetDisplayName() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        BSTR name = nullptr;
        CheckHResult(m_cursor->GetDisplayName(&name));
        return FromBstr(name);
    }

    std::string GetFormattedName(DxcCursorFormatting formatting) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        BSTR name = nullptr;
        CheckHResult(m_cursor->GetFormattedName(formatting, &name));
        return FromBstr(name);
    }

    std::string GetQualifiedName(bool includeTemplateArgs) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        BSTR name = nullptr;
        CheckHResult(m_cursor->GetQualifiedName(includeTemplateArgs, &name));
        return FromBstr(name);
    }

    DxcCursorKind GetKind() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        DxcCursorKind kind = DxcCursorKind::UnexposedDecl;
        CheckHResult(m_cursor->GetKind(&kind));
        return kind;
    }

    DxcCursorKindFlags GetKindFlags() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        DxcCursorKindFlags flags = DxcCursorKindFlags::None;
        CheckHResult(m_cursor->GetKindFlags(&flags));
        return flags;
    }

    DxcCursor GetSemanticParent() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> parent;
        CheckHResult(m_cursor->GetSemanticParent(&parent));
        return DxcCursor(parent.Get());
    }

    DxcCursor GetLexicalParent() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> parent;
        CheckHResult(m_cursor->GetLexicalParent(&parent));
        return DxcCursor(parent.Get());
    }

    DxcType GetCursorType() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcType> type;
        CheckHResult(m_cursor->GetCursorType(&type));
        return DxcType(type.Get());
    }

    int32_t GetNumArguments() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        int32_t result = 0;
        CheckHResult(m_cursor->GetNumArguments(&result));
        return result;
    }

    DxcCursor GetArgumentAt(int32_t index) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> arg;
        CheckHResult(m_cursor->GetArgumentAt(index, &arg));
        return DxcCursor(arg.Get());
    }

    DxcCursor GetReferencedCursor() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> ref;
        CheckHResult(m_cursor->GetReferencedCursor(&ref));
        return DxcCursor(ref.Get());
    }

    DxcCursor GetDefinitionCursor() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> def;
        CheckHResult(m_cursor->GetDefinitionCursor(&def));
        return DxcCursor(def.Get());
    }

    std::vector<DxcCursor> FindReferencesInFile(const DxcFile& file, uint32_t skip, uint32_t top) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }

        IDxcCursor** result = nullptr;
        uint32_t resultLength = 0;
        CheckHResult(m_cursor->FindReferencesInFile(file.GetRaw(), skip, top, &resultLength, &result));

        std::vector<DxcCursor> references;
        references.reserve(resultLength);
        for (uint32_t i = 0; i < resultLength; ++i) {
            references.emplace_back(result[i]);
        }

        CoTaskMemFree(result);
        return references;
    }

    std::string GetSpelling() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        LPSTR spelling = nullptr;
        CheckHResult(m_cursor->GetSpelling(&spelling));
        return FromLpstr(spelling);
    }

    bool IsEqualTo(const DxcCursor& other) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        bool result = false;
        CheckHResult(m_cursor->IsEqualTo(other.m_cursor.Get(), &result));
        return result;
    }

    bool IsNull() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        bool result = false;
        CheckHResult(m_cursor->IsNull(&result));
        return result;
    }

    bool IsDefinition() const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        bool result = false;
        CheckHResult(m_cursor->IsDefinition(&result));
        return result;
    }

    DxcCursor GetSnappedChild(const DxcSourceLocation& location) const {
        if (!m_cursor) {
            throw HassleError("Invalid cursor");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> child;
        CheckHResult(m_cursor->GetSnappedChild(location.GetRaw(), &child));
        return DxcCursor(child.Get());
    }

    std::string GetSource(const std::string& source) const {
        auto range = GetExtent();
        auto offsets = range.GetOffsets();

        if (offsets.startOffset >= source.size() || offsets.endOffset > source.size()) {
            throw HassleError("Source offsets out of range");
        }

        return source.substr(offsets.startOffset, offsets.endOffset - offsets.startOffset);
    }

    IDxcCursor* GetRaw() const { return m_cursor.Get(); }
    bool IsValid() const { return m_cursor != nullptr; }
};

// DxcUnsavedFile wrapper
class DxcUnsavedFile {
private:
    Microsoft::WRL::ComPtr<IDxcUnsavedFile> m_file;

    friend class DxcIndex;

public:
    DxcUnsavedFile() = default;
    explicit DxcUnsavedFile(IDxcUnsavedFile* file) : m_file(file) {}

    uint32_t GetLength() const {
        if (!m_file) {
            throw HassleError("Invalid unsaved file");
        }
        uint32_t length = 0;
        CheckHResult(m_file->GetLength(&length));
        return length;
    }

    IDxcUnsavedFile* GetRaw() const { return m_file.Get(); }
    bool IsValid() const { return m_file != nullptr; }
};

// DxcTranslationUnit wrapper
class DxcTranslationUnit {
private:
    Microsoft::WRL::ComPtr<IDxcTranslationUnit> m_tu;

public:
    DxcTranslationUnit() = default;
    explicit DxcTranslationUnit(IDxcTranslationUnit* tu) : m_tu(tu) {}

    DxcFile GetFile(const std::string& name) const {
        if (!m_tu) {
            throw HassleError("Invalid translation unit");
        }
        Microsoft::WRL::ComPtr<IDxcFile> file;
        CheckHResult(m_tu->GetFile(name.c_str(), &file));
        return DxcFile(file.Get());
    }

    DxcCursor GetCursor() const {
        if (!m_tu) {
            throw HassleError("Invalid translation unit");
        }
        Microsoft::WRL::ComPtr<IDxcCursor> cursor;
        CheckHResult(m_tu->GetCursor(&cursor));
        return DxcCursor(cursor.Get());
    }

    IDxcTranslationUnit* GetRaw() const { return m_tu.Get(); }
    bool IsValid() const { return m_tu != nullptr; }
};

// DxcIndex wrapper
class DxcIndex {
private:
    Microsoft::WRL::ComPtr<IDxcIndex> m_index;

public:
    DxcIndex() = default;
    explicit DxcIndex(IDxcIndex* index) : m_index(index) {}

    DxcTranslationUnit ParseTranslationUnit(
        const std::string& sourceFilename,
        const std::vector<std::string>& args,
        const std::vector<DxcUnsavedFile>& unsavedFiles,
        DxcTranslationUnitFlags options) const
    {
        if (!m_index) {
            throw HassleError("Invalid index");
        }

        // Convert args to C-style strings
        std::vector<const char*> cArgs;
        cArgs.reserve(args.size());
        for (const auto& arg : args) {
            cArgs.push_back(arg.c_str());
        }

        // Convert unsaved files to raw pointers
        std::vector<IDxcUnsavedFile*> rawUnsavedFiles;
        rawUnsavedFiles.reserve(unsavedFiles.size());
        for (const auto& uf : unsavedFiles) {
            rawUnsavedFiles.push_back(uf.GetRaw());
        }

        Microsoft::WRL::ComPtr<IDxcTranslationUnit> tu;
        CheckHResult(m_index->ParseTranslationUnit(
            sourceFilename.c_str(),
            cArgs.data(),
            static_cast<int32_t>(cArgs.size()),
            rawUnsavedFiles.data(),
            static_cast<uint32_t>(rawUnsavedFiles.size()),
            options,
            &tu));

        return DxcTranslationUnit(tu.Get());
    }

    IDxcIndex* GetRaw() const { return m_index.Get(); }
    bool IsValid() const { return m_index != nullptr; }
};

// DxcIntelliSense wrapper
class DxcIntelliSense {
private:
    Microsoft::WRL::ComPtr<IDxcIntelliSense> m_intellisense;

public:
    DxcIntelliSense() = default;
    explicit DxcIntelliSense(IDxcIntelliSense* intellisense) : m_intellisense(intellisense) {}

    DxcTranslationUnitFlags GetDefaultEditingTUOptions() const {
        if (!m_intellisense) {
            throw HassleError("Invalid intellisense");
        }
        DxcTranslationUnitFlags options = DxcTranslationUnitFlags::None;
        CheckHResult(m_intellisense->GetDefaultEditingTUOptions(&options));
        return options;
    }

    DxcIndex CreateIndex() const {
        if (!m_intellisense) {
            throw HassleError("Invalid intellisense");
        }
        Microsoft::WRL::ComPtr<IDxcIndex> index;
        CheckHResult(m_intellisense->CreateIndex(&index));
        return DxcIndex(index.Get());
    }

    DxcUnsavedFile CreateUnsavedFile(const std::string& fileName, const std::string& contents) const {
        if (!m_intellisense) {
            throw HassleError("Invalid intellisense");
        }
        Microsoft::WRL::ComPtr<IDxcUnsavedFile> file;
        CheckHResult(m_intellisense->CreateUnsavedFile(
            fileName.c_str(),
            contents.c_str(),
            static_cast<uint32_t>(contents.size()),
            &file));
        return DxcUnsavedFile(file.Get());
    }

    IDxcIntelliSense* GetRaw() const { return m_intellisense.Get(); }
    bool IsValid() const { return m_intellisense != nullptr; }
};

} // namespace intellisense

// Dxc extension for intellisense
class Dxc;

} // namespace hassle
