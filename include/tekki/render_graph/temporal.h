#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include "tekki/core/result.h"
#include "tekki/backend/vk_sync.h"
#include "tekki/render_graph/types.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/pass_builder.h"

namespace tekki::render_graph {

// Forward declarations
class RenderGraph;
class RetiredRenderGraph;
class Image;
class Buffer;
template<typename T> class Handle;
class ExportableGraphResource;
struct ImageDesc;
struct BufferDesc;
class PassBuilder;
// Device is already defined in types.h


class TemporalResourceKey {
public:
    TemporalResourceKey() = default;
    TemporalResourceKey(const std::string& key) : key_(key) {}
    TemporalResourceKey(const char* key) : key_(key) {}
    
    bool operator==(const TemporalResourceKey& other) const { return key_ == other.key_; }
    
    struct Hash {
        std::size_t operator()(const TemporalResourceKey& key) const {
            return std::hash<std::string>{}(key.key_);
        }
    };
    
private:
    std::string key_;
};

enum class TemporalResourceType {
    Image,
    Buffer
};

class TemporalResource {
public:
    TemporalResource(std::shared_ptr<Image> image) 
        : type_(TemporalResourceType::Image), image_(std::move(image)) {}
    TemporalResource(std::shared_ptr<Buffer> buffer) 
        : type_(TemporalResourceType::Buffer), buffer_(std::move(buffer)) {}
    
    TemporalResourceType GetType() const { return type_; }
    std::shared_ptr<Image> GetImage() const { return image_; }
    std::shared_ptr<Buffer> GetBuffer() const { return buffer_; }
    
    TemporalResource Clone() const {
        if (type_ == TemporalResourceType::Image) {
            return TemporalResource(image_);
        } else {
            return TemporalResource(buffer_);
        }
    }

private:
    TemporalResourceType type_;
    std::shared_ptr<Image> image_;
    std::shared_ptr<Buffer> buffer_;
};

enum class ExportedResourceHandleType {
    Image,
    Buffer
};

class ExportedResourceHandle {
public:
    ExportedResourceHandle(ExportedHandle<Image> handle)
        : type_(ExportedResourceHandleType::Image), image_handle_(handle) {}
    ExportedResourceHandle(ExportedHandle<Buffer> handle)
        : type_(ExportedResourceHandleType::Buffer), buffer_handle_(handle) {}

    ExportedResourceHandleType GetType() const { return type_; }
    ExportedHandle<Image> GetImageHandle() const { return image_handle_; }
    ExportedHandle<Buffer> GetBufferHandle() const { return buffer_handle_; }

private:
    ExportedResourceHandleType type_;
    ExportedHandle<Image> image_handle_{};
    ExportedHandle<Buffer> buffer_handle_{};
};

enum class TemporalResourceStateType {
    Inert,
    Imported,
    Exported
};

class TemporalResourceState {
public:
    struct InertState {
        TemporalResource resource;
        vk_sync::AccessType access_type;
        InertState() : resource(std::shared_ptr<Image>()), access_type(vk_sync::AccessType::None) {}
        InertState(TemporalResource res, vk_sync::AccessType access) : resource(std::move(res)), access_type(access) {}
    };

    struct ImportedState {
        TemporalResource resource;
        std::shared_ptr<ExportableGraphResource> handle;
        ImportedState() : resource(std::shared_ptr<Image>()), handle(nullptr) {}
        ImportedState(TemporalResource res, std::shared_ptr<ExportableGraphResource> h) : resource(std::move(res)), handle(std::move(h)) {}
    };

    struct ExportedState {
        TemporalResource resource;
        ExportedResourceHandle handle;
        ExportedState() : resource(std::shared_ptr<Image>()), handle(ExportedHandle<Image>{}) {}
        ExportedState(TemporalResource res, ExportedResourceHandle h) : resource(std::move(res)), handle(std::move(h)) {}
    };
    
    TemporalResourceState(InertState state)
        : type_(TemporalResourceStateType::Inert), inert_state_(std::move(state)), imported_state_(), exported_state_() {}
    TemporalResourceState(ImportedState state)
        : type_(TemporalResourceStateType::Imported), inert_state_(), imported_state_(std::move(state)), exported_state_() {}
    TemporalResourceState(ExportedState state)
        : type_(TemporalResourceStateType::Exported), inert_state_(), imported_state_(), exported_state_(std::move(state)) {}
    
    TemporalResourceStateType GetType() const { return type_; }
    const InertState& GetInertState() const { return inert_state_; }
    const ImportedState& GetImportedState() const { return imported_state_; }
    const ExportedState& GetExportedState() const { return exported_state_; }
    
    TemporalResourceState CloneAssumingInert() const {
        if (type_ != TemporalResourceStateType::Inert) {
            throw std::runtime_error("Not in inert state!");
        }
        return TemporalResourceState(InertState{
            inert_state_.resource.Clone(),
            inert_state_.access_type
        });
    }

private:
    TemporalResourceStateType type_;
    InertState inert_state_;
    ImportedState imported_state_;
    ExportedState exported_state_;
};

class TemporalRenderGraphState {
public:
    TemporalRenderGraphState() = default;
    
    TemporalRenderGraphState CloneAssumingInert() const {
        TemporalRenderGraphState new_state;
        for (const auto& [key, state] : resources_) {
            new_state.resources_.emplace(key, state.CloneAssumingInert());
        }
        return new_state;
    }
    
    void AddResource(const TemporalResourceKey& key, TemporalResourceState state) {
        resources_.emplace(key, std::move(state));
    }
    
    const TemporalResourceState* GetResource(const TemporalResourceKey& key) const {
        auto it = resources_.find(key);
        return it != resources_.end() ? &it->second : nullptr;
    }
    
    TemporalResourceState* GetResourceMut(const TemporalResourceKey& key) {
        auto it = resources_.find(key);
        return it != resources_.end() ? &it->second : nullptr;
    }
    
    auto begin() { return resources_.begin(); }
    auto end() { return resources_.end(); }
    auto begin() const { return resources_.begin(); }
    auto end() const { return resources_.end(); }

private:
    std::unordered_map<TemporalResourceKey, TemporalResourceState, TemporalResourceKey::Hash> resources_;
};

class ExportedTemporalRenderGraphState {
public:
    ExportedTemporalRenderGraphState(TemporalRenderGraphState state) : state_(std::move(state)) {}
    
    TemporalRenderGraphState RetireTemporal(const RetiredRenderGraph& rg);

private:
    TemporalRenderGraphState state_;
};

class TemporalRenderGraph {
public:
    TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device);

    RenderGraph* GetRenderGraph() { return rg_.get(); }
    const RenderGraph* GetRenderGraph() const { return rg_.get(); }

    Device& GetDevice() const { return *device_; }

    Handle<Image> GetOrCreateTemporalImage(const TemporalResourceKey& key, const ImageDesc& desc);
    Handle<Buffer> GetOrCreateTemporalBuffer(const TemporalResourceKey& key, const BufferDesc& desc);

    // Generic template method that dispatches to the appropriate typed method
    template<typename Desc>
    Handle<typename Desc::Resource> GetOrCreateTemporal(const TemporalResourceKey& key, const Desc& desc) {
        if constexpr (std::is_same_v<Desc, ImageDesc>) {
            return GetOrCreateTemporalImage(key, desc);
        } else if constexpr (std::is_same_v<Desc, BufferDesc>) {
            return GetOrCreateTemporalBuffer(key, desc);
        } else {
            static_assert(std::is_same_v<Desc, ImageDesc> || std::is_same_v<Desc, BufferDesc>,
                         "GetOrCreateTemporal only supports ImageDesc or BufferDesc");
        }
    }

    std::pair<std::unique_ptr<RenderGraph>, ExportedTemporalRenderGraphState> ExportTemporal();

    // Delegate methods to internal RenderGraph
    PassBuilder AddPass(const std::string& name) {
        return rg_->AddPass(name);
    }

    template<typename Desc>
    Handle<typename Desc::Resource> Create(const Desc& desc) {
        return rg_->Create(desc);
    }

private:
    std::unique_ptr<RenderGraph> rg_;
    std::shared_ptr<Device> device_;
    TemporalRenderGraphState temporal_state_;
};

} // namespace tekki::render_graph