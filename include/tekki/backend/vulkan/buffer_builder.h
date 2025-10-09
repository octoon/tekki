#pragma once

#include "device.h"
#include "buffer.h"

#include <vector>
#include <memory>
#include <cstring>

namespace tekki::vulkan {

// Buffer data source interface
class BufferDataSource {
public:
    virtual ~BufferDataSource() = default;
    virtual const uint8_t* as_bytes() const = 0;
    virtual size_t size() const = 0;
    virtual uint64_t alignment() const = 0;
};

// Template implementation for vector data
template<typename T>
class VectorBufferDataSource : public BufferDataSource {
public:
    explicit VectorBufferDataSource(std::vector<T> data)
        : data_(std::move(data)) {}

    const uint8_t* as_bytes() const override {
        return reinterpret_cast<const uint8_t*>(data_.data());
    }

    size_t size() const override {
        return data_.size() * sizeof(T);
    }

    uint64_t alignment() const override {
        return alignof(T);
    }

private:
    std::vector<T> data_;
};

// Template implementation for array data
template<typename T>
class ArrayBufferDataSource : public BufferDataSource {
public:
    ArrayBufferDataSource(const T* data, size_t count)
        : data_(data), count_(count) {}

    const uint8_t* as_bytes() const override {
        return reinterpret_cast<const uint8_t*>(data_);
    }

    size_t size() const override {
        return count_ * sizeof(T);
    }

    uint64_t alignment() const override {
        return alignof(T);
    }

private:
    const T* data_;
    size_t count_;
};

// Pending buffer upload
struct PendingBufferUpload {
    std::unique_ptr<BufferDataSource> source;
    uint64_t offset;
};

// Buffer builder for staged buffer uploads
class BufferBuilder {
public:
    BufferBuilder() = default;

    // Get current offset
    uint64_t current_offset() const {
        return current_offset_;
    }

    // Append data from a vector
    template<typename T>
    uint64_t append(std::vector<T> data) {
        auto source = std::make_unique<VectorBufferDataSource<T>>(std::move(data));
        return append_impl(std::move(source));
    }

    // Append data from an array
    template<typename T>
    uint64_t append(const T* data, size_t count) {
        auto source = std::make_unique<ArrayBufferDataSource<T>>(data, count);
        return append_impl(std::move(source));
    }

    // Upload all pending data to target buffer
    void upload(
        Device* device,
        Buffer* target,
        uint64_t target_offset = 0
    );

private:
    uint64_t append_impl(std::unique_ptr<BufferDataSource> source) {
        uint64_t alignment = source->alignment();

        // Ensure alignment is power of 2
        assert((alignment & (alignment - 1)) == 0);

        // Align current offset
        uint64_t data_start = (current_offset_ + alignment - 1) & ~(alignment - 1);
        uint64_t data_len = source->size();

        pending_uploads_.push_back(PendingBufferUpload{
            std::move(source),
            data_start
        });

        current_offset_ = data_start + data_len;
        return data_start;
    }

    std::vector<PendingBufferUpload> pending_uploads_;
    uint64_t current_offset_ = 0;
};

} // namespace tekki::vulkan
