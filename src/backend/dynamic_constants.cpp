#include "tekki/backend/dynamic_constants.h"
#include "tekki/backend/Buffer.h"
#include "tekki/backend/Device.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <cstring>

namespace tekki::backend {

DynamicConstants::DynamicConstants(std::shared_ptr<Buffer> buffer)
    : buffer(buffer)
    , frameOffsetBytes(0)
    , frameParity(0) {
}

void DynamicConstants::AdvanceFrame() {
    frameParity = (frameParity + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT;
    frameOffsetBytes = 0;
}

uint32_t DynamicConstants::CurrentOffset() const {
    return static_cast<uint32_t>(frameParity * DYNAMIC_CONSTANTS_SIZE_BYTES + frameOffsetBytes);
}

VkDeviceAddress DynamicConstants::CurrentDeviceAddress(const Device& device) const {
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

// Explicit template instantiations for common types
template uint32_t DynamicConstants::Push<int>(const int&);
template uint32_t DynamicConstants::Push<float>(const float&);
template uint32_t DynamicConstants::Push<double>(const double&);
template uint32_t DynamicConstants::Push<glm::vec2>(const glm::vec2&);
template uint32_t DynamicConstants::Push<glm::vec3>(const glm::vec3&);
template uint32_t DynamicConstants::Push<glm::vec4>(const glm::vec4&);
template uint32_t DynamicConstants::Push<glm::mat3>(const glm::mat3&);
template uint32_t DynamicConstants::Push<glm::mat4>(const glm::mat4&);

template uint32_t DynamicConstants::PushFromIter<int, std::vector<int>::iterator>(std::vector<int>::iterator);
template uint32_t DynamicConstants::PushFromIter<float, std::vector<float>::iterator>(std::vector<float>::iterator);
template uint32_t DynamicConstants::PushFromIter<double, std::vector<double>::iterator>(std::vector<double>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::vec2, std::vector<glm::vec2>::iterator>(std::vector<glm::vec2>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::vec3, std::vector<glm::vec3>::iterator>(std::vector<glm::vec3>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::vec4, std::vector<glm::vec4>::iterator>(std::vector<glm::vec4>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::mat3, std::vector<glm::mat3>::iterator>(std::vector<glm::mat3>::iterator);
template uint32_t DynamicConstants::PushFromIter<glm::mat4, std::vector<glm::mat4>::iterator>(std::vector<glm::mat4>::iterator);

} // namespace tekki::backend