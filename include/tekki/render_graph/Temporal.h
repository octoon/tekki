#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "../backend/vulkan/device.h"
#include "Graph.h"

namespace tekki::render_graph
{

// Read-only handle wrapper
template <typename ResType> class ReadOnlyHandle
{
public:
    Handle<ResType> handle;

    const Handle<ResType>* operator->() const { return &handle; }
    const Handle<ResType>& operator*() const { return handle; }
};

template <typename ResType> ReadOnlyHandle<ResType> make_read_only_handle(Handle<ResType> handle)
{
    return ReadOnlyHandle<ResType>{handle};
}

// Temporal resource key
struct TemporalResourceKey
{
    std::string name;

    TemporalResourceKey(const std::string& name) : name(name) {}
    TemporalResourceKey(const char* name) : name(name) {}

    bool operator==(const TemporalResourceKey& other) const { return name == other.name; }

    struct Hash
    {
        size_t operator()(const TemporalResourceKey& key) const { return std::hash<std::string>{}(key.name); }
    };
};

// Temporal resource types
struct TemporalResource
{
    enum class Type
    {
        Image,
        Buffer
    };

    Type type;
    std::variant<std::shared_ptr<Image>, std::shared_ptr<Buffer>> data;

    TemporalResource clone() const { return {type, data}; }
};

// Exported resource handle
struct ExportedResourceHandle
{
    enum class Type
    {
        Image,
        Buffer
    };

    Type type;
    std::variant<ExportedHandle<ImageResource>, ExportedHandle<BufferResource>> data;
};

// Temporal resource state
struct TemporalResourceState
{
    enum class Type
    {
        Inert,
        Imported,
        Exported
    };

    Type type;
    std::variant < struct
    {
        TemporalResource resource;
        vk_sync::AccessType access_type;
    }, struct
    {
        TemporalResource resource;
        ExportableGraphResource handle;
    }, struct
    {
        TemporalResource resource;
        ExportedResourceHandle handle;
    } > data;
};

// Temporal render graph state
class TemporalRenderGraphState
{
public:
    std::unordered_map<TemporalResourceKey, TemporalResourceState, TemporalResourceKey::Hash> resources;

    TemporalRenderGraphState clone_assuming_inert() const
    {
        TemporalRenderGraphState result;
        for (const auto& [key, state] : resources)
        {
            if (state.type == TemporalResourceState::Type::Inert)
            {
                auto& inert_data = std::get<0>(state.data);
                result.resources[key] = TemporalResourceState{
                    .type = TemporalResourceState::Type::Inert,
                    .data = {.resource = inert_data.resource.clone(), .access_type = inert_data.access_type}};
            }
            else
            {
                throw std::runtime_error("Not in inert state!");
            }
        }
        return result;
    }
};

// Exported temporal state
struct ExportedTemporalRenderGraphState
{
    TemporalRenderGraphState state;
};

// Temporal render graph
class TemporalRenderGraph
{
public:
    TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device);

    // Dereference to underlying render graph
    RenderGraph& operator*() { return rg; }
    const RenderGraph& operator*() const { return rg; }
    RenderGraph* operator->() { return &rg; }
    const RenderGraph* operator->() const { return &rg; }

    // Device access
    Device* device() { return device_.get(); }

    // Temporal resource management
    template <typename Desc>
    Handle<typename Desc::ResourceType> get_or_create_temporal(const TemporalResourceKey& key, const Desc& desc);

    // Export temporal state
    std::pair<RenderGraph, ExportedTemporalRenderGraphState> export_temporal();

private:
    RenderGraph rg;
    std::shared_ptr<Device> device_;
    TemporalRenderGraphState temporal_state;
};

// Get or create trait
template <typename Desc> class GetOrCreateTemporal
{
public:
    static Handle<typename Desc::ResourceType> get_or_create_temporal(TemporalRenderGraph& self,
                                                                      const TemporalResourceKey& key, const Desc& desc);
};

// Specialization for ImageDesc
template <> class GetOrCreateTemporal<ImageDesc>
{
public:
    static Handle<ImageResource> get_or_create_temporal(TemporalRenderGraph& self, const TemporalResourceKey& key,
                                                        const ImageDesc& desc);
};

// Specialization for BufferDesc
template <> class GetOrCreateTemporal<BufferDesc>
{
public:
    static Handle<BufferResource> get_or_create_temporal(TemporalRenderGraph& self, const TemporalResourceKey& key,
                                                         const BufferDesc& desc);
};

// Retire temporal state
TemporalRenderGraphState retire_temporal(const ExportedTemporalRenderGraphState& exported_state,
                                         const RetiredRenderGraph& rg);

} // namespace tekki::render_graph