#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace tekki::backend {

// A temporary list that grows in chunks to maintain stable references
// This is similar to Rust's TempList implementation using ArrayVec
template <typename T, size_t ChunkSize = 8>
class ChunkyList {
private:
    struct ChunkyListInner {
        std::vector<T> payload;
        std::unique_ptr<ChunkyListInner> next;

        ChunkyListInner() {
            payload.reserve(ChunkSize);
        }
    };

    std::unique_ptr<ChunkyListInner> inner_;

public:
    ChunkyList() : inner_(std::make_unique<ChunkyListInner>()) {}

    // Add an item to the list and return a stable reference to it
    // The reference remains valid for the lifetime of the ChunkyList
    T* Add(T&& item) {
        if (inner_->payload.size() < ChunkSize) {
            inner_->payload.push_back(std::move(item));
            return &inner_->payload.back();
        } else {
            // Current chunk is full, create a new one
            auto new_inner = std::make_unique<ChunkyListInner>();
            new_inner->payload.push_back(std::move(item));

            // Swap the new inner with the current one
            new_inner->next = std::move(inner_);
            inner_ = std::move(new_inner);

            return &inner_->payload.back();
        }
    }

    // Add an item by copy
    T* Add(const T& item) {
        T copy = item;
        return Add(std::move(copy));
    }

    // Clear all items
    void Clear() {
        inner_ = std::make_unique<ChunkyListInner>();
    }
};

} // namespace tekki::backend
