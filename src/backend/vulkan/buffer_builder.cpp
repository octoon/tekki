#include "../../../include/tekki/backend/vulkan/buffer_builder.h"
#include "../../../include/tekki/core/log.h"

namespace tekki::vulkan {

void BufferBuilder::upload(
    Device* device,
    Buffer* target,
    uint64_t target_offset
) {
    // Calculate total size needed
    size_t total_size = 0;
    for (const auto& upload : pending_uploads_) {
        total_size += upload.source->size();
    }

    // Verify target buffer can hold all data
    assert(total_size + target_offset <= target->desc().size);

    // Create staging buffer
    constexpr size_t STAGING_BYTES = 16 * 1024 * 1024; // 16 MB
    auto staging_buffer = device->create_buffer(
        BufferDesc::new_cpu_to_gpu(
            STAGING_BYTES,
            vk::BufferUsageFlagBits::eTransferSrc
        ),
        "BufferBuilder staging"
    );

    // Process uploads in chunks
    for (const auto& upload : pending_uploads_) {
        const uint8_t* src_data = upload.source->as_bytes();
        size_t byte_count = upload.source->size();
        size_t src_offset = 0;

        while (src_offset < byte_count) {
            size_t chunk_size = std::min(byte_count - src_offset, STAGING_BYTES);

            // Copy to staging buffer
            auto mapped_data = staging_buffer->mapped_data<uint8_t>();
            if (mapped_data) {
                std::memcpy(mapped_data->data(), src_data + src_offset, chunk_size);
            }

            // Submit copy command
            device->with_setup_cb([&](vk::CommandBuffer cb) {
                vk::BufferCopy copy_region{
                    0,  // src_offset
                    target_offset + upload.offset + src_offset,  // dst_offset
                    chunk_size  // size
                };

                cb.copyBuffer(staging_buffer->raw(), target->raw(), 1, &copy_region);
            });

            src_offset += chunk_size;
        }
    }

    // Clear pending uploads
    pending_uploads_.clear();
    current_offset_ = 0;
}

} // namespace tekki::vulkan
