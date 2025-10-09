#include "tekki/renderer/buffer_builder.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <glm/glm.hpp>
#include <algorithm>

namespace tekki::renderer {

uint64_t BufferBuilder::GetCurrentOffset() const {
    return m_currentOffset;
}

template<typename T>
uint64_t BufferBuilder::Append(const std::vector<T>& data) {
    auto source = std::make_shared<VectorBufferDataSource<T>>(data);
    return AppendInternal(source);
}

template<typename T>
uint64_t BufferBuilder::Append(const T* data, size_t size) {
    auto source = std::make_shared<StaticSliceBufferDataSource<T>>(data, size);
    return AppendInternal(source);
}

void BufferBuilder::Upload(Device* device, Buffer* target, uint64_t targetOffset) {
    if (!device || !target) {
        throw std::invalid_argument("Device and target buffer must not be null");
    }

    size_t totalSize = 0;
    for (const auto& upload : m_pendingUploads) {
        totalSize += upload.Source->GetSize();
    }

    if (totalSize + targetOffset > target->GetSize()) {
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
            chunks.push_back({i, glm::uvec2(start, end)});
        }
    }

    for (const auto& chunk : chunks) {
        const auto& pending = m_pendingUploads[chunk.PendingIndex];
        size_t chunkSize = chunk.SourceRange.y - chunk.SourceRange.x;
        
        auto stagingBuffer = device->CreateStagingBuffer(STAGING_BYTES);
        if (!stagingBuffer) {
            throw std::runtime_error("Failed to create staging buffer");
        }

        const uint8_t* sourceData = pending.Source->AsBytes() + chunk.SourceRange.x;
        stagingBuffer->CopyFrom(sourceData, chunkSize, 0);

        try {
            device->CopyBuffer(
                stagingBuffer.get(),
                target,
                0,
                targetOffset + pending.Offset + chunk.SourceRange.x,
                chunkSize
            );
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Buffer copy failed: ") + e.what());
        }
    }
}

uint64_t BufferBuilder::AppendInternal(std::shared_ptr<BufferDataSource> source) {
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

} // namespace tekki::renderer