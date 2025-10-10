#include "tekki/gpu_allocator/allocator.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace tekki {

// AllocationSizes 实现
AllocationSizes::AllocationSizes(uint64_t device_memblock_size, uint64_t host_memblock_size)
    : min_device_memblock_size_(AdjustMemblockSize(device_memblock_size, "Device")),
      max_device_memblock_size_(AdjustMemblockSize(device_memblock_size, "Device")),
      min_host_memblock_size_(AdjustMemblockSize(host_memblock_size, "Host")),
      max_host_memblock_size_(AdjustMemblockSize(host_memblock_size, "Host")) {}

AllocationSizes& AllocationSizes::WithMaxDeviceMemblockSize(uint64_t size) {
    max_device_memblock_size_ = std::max(
        AdjustMemblockSize(size, "Device"),
        min_device_memblock_size_
    );
    return *this;
}

AllocationSizes& AllocationSizes::WithMaxHostMemblockSize(uint64_t size) {
    max_host_memblock_size_ = std::max(
        AdjustMemblockSize(size, "Host"),
        min_host_memblock_size_
    );
    return *this;
}

uint64_t AllocationSizes::GetMemblockSize(bool is_host, size_t count) const {
    uint64_t min_size = is_host ? min_host_memblock_size_ : min_device_memblock_size_;
    uint64_t max_size = is_host ? max_host_memblock_size_ : max_device_memblock_size_;

    // 限制移位次数避免溢出
    size_t shift = std::min<size_t>(count, 7);
    return std::min(min_size << shift, max_size);
}

uint64_t AllocationSizes::AdjustMemblockSize(uint64_t size, const char* kind) {
    constexpr uint64_t MIN_SIZE = 4 * MB;
    constexpr uint64_t MAX_SIZE = 256 * MB;

    size = std::clamp(size, MIN_SIZE, MAX_SIZE);

    if (size % (4 * MB) == 0) {
        return size;
    }

    uint64_t val = size / (4 * MB) + 1;
    uint64_t new_size = val * 4 * MB;
    spdlog::warn("{} memory block size must be a multiple of 4MB, clamping to {}MB",
                 kind, new_size / MB);

    return new_size;
}

// 格式化字节数
std::string FormatBytes(uint64_t amount) {
    static const char* SUFFIX[] = {"B", "KB", "MB", "GB", "TB"};

    size_t idx = 0;
    double print_amount = static_cast<double>(amount);

    while (amount >= 1024 && idx < 4) {
        print_amount = static_cast<double>(amount) / 1024.0;
        amount /= 1024;
        ++idx;
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f %s", print_amount, SUFFIX[idx]);
    return std::string(buffer);
}

} // namespace tekki
