#pragma once

#include <memory>
#include <cstdint>
#include "tekki/render_graph/types.h"

namespace tekki::render_graph {

// Forward declarations
class Graph;
class ResourceRegistry;

// Resource handle template matching Rust version
template<typename T>
class Handle {
public:
    GraphRawResourceHandle raw{0, 0};
    typename T::Desc desc{};

    Handle() = default;
    Handle(GraphRawResourceHandle r, typename T::Desc d) : raw(r), desc(d) {}
    explicit Handle(uint64_t id) : raw{static_cast<uint32_t>(id), 0} {}

    uint64_t id() const { return raw.id; }
    bool is_valid() const { return raw.is_valid(); }

    // Accessor for desc (matching Rust API)
    const typename T::Desc& Desc() const { return desc; }
    typename T::Desc& Desc() { return desc; }

    // Pointer-like access for convenience (returns desc)
    const typename T::Desc* operator->() const { return &desc; }
    typename T::Desc* operator->() { return &desc; }

    bool operator==(const Handle& other) const { return raw == other.raw; }
    bool operator!=(const Handle& other) const { return !(raw == other.raw); }
    bool operator<(const Handle& other) const { return raw.id < other.raw.id; }

    GraphRawResourceHandle next_version() const {
        return raw.next_version();
    }
};

// Read-only resource handle
template<typename T>
class ReadOnlyHandle {
public:
    ReadOnlyHandle() : id_(0) {}
    explicit ReadOnlyHandle(uint64_t id) : id_(id) {}
    ReadOnlyHandle(const Handle<T>& handle) : id_(handle.id()) {}

    uint64_t id() const { return id_; }
    bool is_valid() const { return id_ != 0; }

    bool operator==(const ReadOnlyHandle& other) const { return id_ == other.id_; }
    bool operator!=(const ReadOnlyHandle& other) const { return id_ != other.id_; }
    bool operator<(const ReadOnlyHandle& other) const { return id_ < other.id_; }

private:
    uint64_t id_;
};

// Extended Ref implementation for pass_builder - matching Rust version
template<typename Res, typename ViewType>
struct Ref {
    GraphRawResourceHandle handle;
    typename Res::Desc desc;
};

// Simple Ref type for resources
template<typename T>
using SimpleRef = Handle<T>;

} // namespace tekki::render_graph
