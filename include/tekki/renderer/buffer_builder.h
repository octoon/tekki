#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <glm/glm.hpp>
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/buffer.h"

namespace tekki::renderer {

// Use full types from backend
using Device = tekki::backend::vulkan::Device;
using Buffer = tekki::backend::vulkan::Buffer;

class BufferDataSource {
public:
    virtual ~BufferDataSource() = default;
    virtual const uint8_t* AsBytes() const = 0;
    virtual size_t GetSize() const = 0;
    virtual uint64_t GetAlignment() const = 0;
};

template<typename T>
class StaticSliceBufferDataSource : public BufferDataSource {
private:
    const T* m_data;
    size_t m_size;

public:
    StaticSliceBufferDataSource(const T* data, size_t size) : m_data(data), m_size(size) {}

    const uint8_t* AsBytes() const override {
        return reinterpret_cast<const uint8_t*>(m_data);
    }

    size_t GetSize() const override {
        return m_size * sizeof(T);
    }

    uint64_t GetAlignment() const override {
        return alignof(T);
    }
};

template<typename T>
class VectorBufferDataSource : public BufferDataSource {
private:
    const std::vector<T>& m_data;

public:
    VectorBufferDataSource(const std::vector<T>& data) : m_data(data) {}

    const uint8_t* AsBytes() const override {
        return reinterpret_cast<const uint8_t*>(m_data.data());
    }

    size_t GetSize() const override {
        return m_data.size() * sizeof(T);
    }

    uint64_t GetAlignment() const override {
        return alignof(T);
    }
};

struct PendingBufferUpload {
    std::shared_ptr<BufferDataSource> Source;
    uint64_t Offset;
};

class BufferBuilder {
private:
    std::vector<PendingBufferUpload> m_pendingUploads;
    uint64_t m_currentOffset;

public:
    BufferBuilder() : m_currentOffset(0) {}

    uint64_t GetCurrentOffset() const {
        return m_currentOffset;
    }

    template<typename T>
    uint64_t Append(const std::vector<T>& data) {
        auto source = std::make_shared<VectorBufferDataSource<T>>(data);
        return AppendInternal(source);
    }

    template<typename T>
    uint64_t Append(const T* data, size_t size) {
        auto source = std::make_shared<StaticSliceBufferDataSource<T>>(data, size);
        return AppendInternal(source);
    }

    void Upload(std::shared_ptr<Device> device, Buffer* target, uint64_t targetOffset) {
        if (!device || !target) {
            throw std::invalid_argument("Device and target buffer must not be null");
        }

        size_t totalSize = 0;
        for (const auto& upload : m_pendingUploads) {
            totalSize += upload.Source->GetSize();
        }

        if (totalSize + targetOffset > target->desc.size) {
            throw std::runtime_error("Buffer upload would exceed target buffer size");
        }

        const size_t STAGING_BYTES = 16 * 1024 * 1024;

        struct UploadChunk {
            size_t PendingIndex;
            glm::uvec2 SourceRange;
        };

        std::vector<UploadChunk> chunks;

        for (size_t i = 0; i < m_pendingUploads.size(); ++i) {
            const auto& pending = m_pendingUploads[i];
            size_t byteCount = pending.Source->GetSize();
            size_t chunkCount = (byteCount + STAGING_BYTES - 1) / STAGING_BYTES;

            for (size_t chunk = 0; chunk < chunkCount; ++chunk) {
                size_t start = chunk * STAGING_BYTES;
                size_t end = std::min((chunk + 1) * STAGING_BYTES, byteCount);
                chunks.push_back({i, glm::uvec2(static_cast<uint32_t>(start), static_cast<uint32_t>(end))});
            }
        }

        for (const auto& chunk : chunks) {
            const auto& pending = m_pendingUploads[chunk.PendingIndex];
            size_t chunkSize = chunk.SourceRange.y - chunk.SourceRange.x;

            // Create staging buffer
            tekki::backend::vulkan::BufferDesc stagingDesc;
            stagingDesc.size = chunkSize;
            stagingDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingDesc.memory_location = tekki::MemoryLocation::CpuToGpu;

            auto stagingBuffer = device->CreateBuffer(stagingDesc, "buffer upload staging");

            // Copy data to staging buffer
            const uint8_t* sourceData = pending.Source->AsBytes() + chunk.SourceRange.x;
            auto mappedSlice = stagingBuffer.Allocation.MappedSlice();
            if (!mappedSlice) {
                throw std::runtime_error("Failed to map staging buffer");
            }
            std::memcpy(mappedSlice, sourceData, chunkSize);

            // Copy from staging to target buffer
            device->WithSetupCb([&](VkCommandBuffer cb) {
                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = targetOffset + pending.Offset + chunk.SourceRange.x;
                copyRegion.size = chunkSize;
                vkCmdCopyBuffer(cb, stagingBuffer.Raw, target->Raw, 1, &copyRegion);
            });

            // Clean up staging buffer
            device->ImmediateDestroyBuffer(std::move(stagingBuffer));
        }
    }

private:
    uint64_t AppendInternal(std::shared_ptr<BufferDataSource> source) {
        uint64_t alignment = source->GetAlignment();
        assert((alignment & (alignment - 1)) == 0 && "Alignment must be power of two");

        uint64_t dataStart = (m_currentOffset + alignment - 1) & ~(alignment - 1);
        uint64_t dataLen = source->GetSize();

        m_pendingUploads.push_back(PendingBufferUpload{
            std::move(source),
            dataStart
        });
        m_currentOffset = dataStart + dataLen;

        return dataStart;
    }
};

} // namespace tekki::renderer