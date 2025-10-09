#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include "vulkan/vulkan.h"
#include "tekki/core/Result.h"

namespace tekki::backend {

constexpr size_t DYNAMIC_CONSTANTS_SIZE_BYTES = 1024 * 1024 * 16;
constexpr size_t DYNAMIC_CONSTANTS_BUFFER_COUNT = 2;

// Generally supported minimum uniform buffer size across vendors (maxUniformBufferRange)
// Could be bumped to 65536 if needed.
constexpr size_t MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH = 16384;

// Must be >= `minUniformBufferOffsetAlignment`. In practice <= 256.
constexpr size_t DYNAMIC_CONSTANTS_ALIGNMENT = 256;

// Sadly we can't have unsized dynamic storage buffers sub-allocated from dynamic constants because WHOLE_SIZE blows up.
// https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/2846#issuecomment-851744837
// For now, just a max size.
constexpr size_t MAX_DYNAMIC_CONSTANTS_STORAGE_BUFFER_BYTES = 1024 * 1024;

class Buffer;

class DynamicConstants {
public:
    DynamicConstants(std::shared_ptr<Buffer> buffer);
    
    void AdvanceFrame();
    
    uint32_t CurrentOffset() const;
    
    VkDeviceAddress CurrentDeviceAddress(const class Device& device) const;
    
    template<typename T>
    uint32_t Push(const T& t);
    
    template<typename T, typename Iter>
    uint32_t PushFromIter(Iter iter);

    std::shared_ptr<Buffer> buffer;

private:
    size_t frameOffsetBytes;
    size_t frameParity;
};

inline DynamicConstants::DynamicConstants(std::shared_ptr<Buffer> buffer)
    : buffer(buffer)
    , frameOffsetBytes(0)
    , frameParity(0) {
}

inline void DynamicConstants::AdvanceFrame() {
    frameParity = (frameParity + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT;
    frameOffsetBytes = 0;
}

inline uint32_t DynamicConstants::CurrentOffset() const {
    return static_cast<uint32_t>(frameParity * DYNAMIC_CONSTANTS_SIZE_BYTES + frameOffsetBytes);
}

inline VkDeviceAddress DynamicConstants::CurrentDeviceAddress(const class Device& device) const {
    return buffer->DeviceAddress(device) + static_cast<VkDeviceAddress>(CurrentOffset());
}

template<typename T>
uint32_t DynamicConstants::Push(const T& t) {
    const size_t tSize = sizeof(T);
    if (frameOffsetBytes + tSize >= DYNAMIC_CONSTANTS_SIZE_BYTES) {
        throw std::runtime_error("Dynamic constants buffer overflow");
    }

    uint32_t bufferOffset = CurrentOffset();
    auto mappedSlice = buffer->GetAllocation().GetMappedSlice();
    if (!mappedSlice) {
        throw std::runtime_error("Failed to get mapped slice from buffer allocation");
    }

    uint8_t* dst = mappedSlice->data() + bufferOffset;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(&t);
    std::copy(src, src + tSize, dst);

    const size_t tSizeAligned = (tSize + DYNAMIC_CONSTANTS_ALIGNMENT - 1) & ~(DYNAMIC_CONSTANTS_ALIGNMENT - 1);
    frameOffsetBytes += tSizeAligned;

    return bufferOffset;
}

template<typename T, typename Iter>
uint32_t DynamicConstants::PushFromIter(Iter iter) {
    const size_t tSize = sizeof(T);
    const size_t tAlign = alignof(T);

    if (frameOffsetBytes + tSize >= DYNAMIC_CONSTANTS_SIZE_BYTES) {
        throw std::runtime_error("Dynamic constants buffer overflow");
    }
    if (DYNAMIC_CONSTANTS_ALIGNMENT % tAlign != 0) {
        throw std::runtime_error("Alignment requirement not satisfied");
    }

    uint32_t bufferOffset = CurrentOffset();
    if (bufferOffset % tAlign != 0) {
        throw std::runtime_error("Buffer offset not properly aligned");
    }

    auto mappedSlice = buffer->GetAllocation().GetMappedSlice();
    if (!mappedSlice) {
        throw std::runtime_error("Failed to get mapped slice from buffer allocation");
    }

    size_t dstOffset = bufferOffset;
    for (const T& t : iter) {
        uint8_t* dst = mappedSlice->data() + dstOffset;
        const uint8_t* src = reinterpret_cast<const uint8_t*>(&t);
        std::copy(src, src + tSize, dst);
        dstOffset += tSize + tAlign - 1;
        dstOffset &= ~(tAlign - 1);
    }

    frameOffsetBytes += dstOffset - bufferOffset;
    frameOffsetBytes += DYNAMIC_CONSTANTS_ALIGNMENT - 1;
    frameOffsetBytes &= ~(DYNAMIC_CONSTANTS_ALIGNMENT - 1);

    return bufferOffset;
}

} // namespace tekki::backend