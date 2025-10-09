#include "../../include/tekki/core/mmap.h"
#include "../../include/tekki/core/log.h"

namespace tekki::core {

// MemoryMappedFile implementation

MemoryMappedFile::~MemoryMappedFile() {
    close();
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
    : data_(other.data_)
    , size_(other.size_)
#ifdef _WIN32
    , file_handle_(other.file_handle_)
    , mapping_handle_(other.mapping_handle_)
#else
    , file_descriptor_(other.file_descriptor_)
#endif
{
    // Clear moved-from object
    other.data_ = nullptr;
    other.size_ = 0;
#ifdef _WIN32
    other.file_handle_ = INVALID_HANDLE_VALUE;
    other.mapping_handle_ = INVALID_HANDLE_VALUE;
#else
    other.file_descriptor_ = -1;
#endif
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
    if (this != &other) {
        close();

        data_ = other.data_;
        size_ = other.size_;
#ifdef _WIN32
        file_handle_ = other.file_handle_;
        mapping_handle_ = other.mapping_handle_;
        other.file_handle_ = INVALID_HANDLE_VALUE;
        other.mapping_handle_ = INVALID_HANDLE_VALUE;
#else
        file_descriptor_ = other.file_descriptor_;
        other.file_descriptor_ = -1;
#endif

        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

bool MemoryMappedFile::open(const std::filesystem::path& file_path) {
    close(); // Close any existing mapping

    if (!std::filesystem::exists(file_path)) {
        TEKKI_LOG_ERROR("Memory map failed: file does not exist: {}", file_path.string());
        return false;
    }

    const auto file_size = std::filesystem::file_size(file_path);
    if (file_size == 0) {
        TEKKI_LOG_ERROR("Memory map failed: file is empty: {}", file_path.string());
        return false;
    }

#ifdef _WIN32
    // Windows implementation
    file_handle_ = CreateFileW(
        file_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file_handle_ == INVALID_HANDLE_VALUE) {
        TEKKI_LOG_ERROR("Memory map failed: cannot open file: {}", file_path.string());
        return false;
    }

    mapping_handle_ = CreateFileMappingW(
        file_handle_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );

    if (mapping_handle_ == nullptr) {
        TEKKI_LOG_ERROR("Memory map failed: cannot create file mapping: {}", file_path.string());
        cleanup();
        return false;
    }

    data_ = MapViewOfFile(
        mapping_handle_,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    if (data_ == nullptr) {
        TEKKI_LOG_ERROR("Memory map failed: cannot map view of file: {}", file_path.string());
        cleanup();
        return false;
    }

    size_ = static_cast<size_t>(file_size);

#else
    // POSIX implementation
    file_descriptor_ = ::open(file_path.c_str(), O_RDONLY);
    if (file_descriptor_ == -1) {
        TEKKI_LOG_ERROR("Memory map failed: cannot open file: {}", file_path.string());
        return false;
    }

    data_ = ::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, file_descriptor_, 0);
    if (data_ == MAP_FAILED) {
        TEKKI_LOG_ERROR("Memory map failed: mmap failed: {}", file_path.string());
        data_ = nullptr;
        cleanup();
        return false;
    }

    size_ = file_size;
#endif

    TEKKI_LOG_DEBUG("Successfully memory mapped file: {} ({} bytes)", file_path.string(), size_);
    return true;
}

void MemoryMappedFile::close() {
    if (data_ != nullptr) {
        cleanup();
    }
}

void MemoryMappedFile::cleanup() {
#ifdef _WIN32
    if (data_ != nullptr) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(mapping_handle_);
        mapping_handle_ = INVALID_HANDLE_VALUE;
    }
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (data_ != nullptr && data_ != MAP_FAILED) {
        ::munmap(const_cast<void*>(data_), size_);
        data_ = nullptr;
    }
    if (file_descriptor_ != -1) {
        ::close(file_descriptor_);
        file_descriptor_ = -1;
    }
#endif
    size_ = 0;
}

// AssetMmapCache implementation

AssetMmapCache& AssetMmapCache::instance() {
    static AssetMmapCache instance;
    return instance;
}

void AssetMmapCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    TEKKI_LOG_DEBUG("Clearing asset mmap cache ({} entries)", mmaps_.size());
    mmaps_.clear();
}

} // namespace tekki::core