#include "tekki/render_graph/temporal.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <utility>
#include "tekki/core/Result.h"
#include "tekki/backend/vk_sync.h"
#include "tekki/render_graph/RenderGraph.h"
#include "tekki/render_graph/Resource.h"
#include "tekki/render_graph/Buffer.h"
#include "tekki/render_graph/Image.h"

namespace tekki::render_graph {

template<typename ResType>
ReadOnlyHandle<ResType>::ReadOnlyHandle(Handle<ResType> handle) : handle_(std::move(handle)) {}

template<typename ResType>
const Handle<ResType>& ReadOnlyHandle<ResType>::operator*() const { return handle_; }

template<typename ResType>
const Handle<ResType>* ReadOnlyHandle<ResType>::operator->() const { return &handle_; }

TemporalResourceKey::TemporalResourceKey(const std::string& key) : key_(key) {}

TemporalResourceKey::TemporalResourceKey(const char* key) : key_(key) {}

bool TemporalResourceKey::operator==(const TemporalResourceKey& other) const { return key_ == other.key_; }

std::size_t TemporalResourceKey::Hash::operator()(const TemporalResourceKey& key) const {
    return std::hash<std::string>{}(key.key_);
}

TemporalResource::TemporalResource(std::shared_ptr<Image> image) 
    : type_(TemporalResourceType::Image), image_(std::move(image)) {}

TemporalResource::TemporalResource(std::shared_ptr<Buffer> buffer) 
    : type_(TemporalResourceType::Buffer), buffer_(std::move(buffer)) {}

TemporalResourceType TemporalResource::GetType() const { return type_; }

std::shared_ptr<Image> TemporalResource::GetImage() const { return image_; }

std::shared_ptr<Buffer> TemporalResource::GetBuffer() const { return buffer_; }

TemporalResource TemporalResource::Clone() const {
    if (type_ == TemporalResourceType::Image) {
        return TemporalResource(image_);
    } else {
        return TemporalResource(buffer_);
    }
}

ExportedResourceHandle::ExportedResourceHandle(ExportedHandle<Image> handle) 
    : type_(ExportedResourceHandleType::Image), image_handle_(handle) {}

ExportedResourceHandle::ExportedResourceHandle(ExportedHandle<Buffer> handle) 
    : type_(ExportedResourceHandleType::Buffer), buffer_handle_(handle) {}

ExportedResourceHandleType ExportedResourceHandle::GetType() const { return type_; }

ExportedHandle<Image> ExportedResourceHandle::GetImageHandle() const { return image_handle_; }

ExportedHandle<Buffer> ExportedResourceHandle::GetBufferHandle() const { return buffer_handle_; }

TemporalResourceState::TemporalResourceState(InertState state) 
    : type_(TemporalResourceStateType::Inert), inert_state_(std::move(state)) {}

TemporalResourceState::TemporalResourceState(ImportedState state) 
    : type_(TemporalResourceStateType::Imported), imported_state_(std::move(state)) {}

TemporalResourceState::TemporalResourceState(ExportedState state) 
    : type_(TemporalResourceStateType::Exported), exported_state_(std::move(state)) {}

TemporalResourceStateType TemporalResourceState::GetType() const { return type_; }

const TemporalResourceState::InertState& TemporalResourceState::GetInertState() const { return inert_state_; }

const TemporalResourceState::ImportedState& TemporalResourceState::GetImportedState() const { return imported_state_; }

const TemporalResourceState::ExportedState& TemporalResourceState::GetExportedState() const { return exported_state_; }

TemporalResourceState TemporalResourceState::CloneAssumingInert() const {
    if (type_ != TemporalResourceStateType::Inert) {
        throw std::runtime_error("Not in inert state!");
    }
    return TemporalResourceState(InertState{
        inert_state_.resource.Clone(),
        inert_state_.access_type
    });
}

TemporalRenderGraphState TemporalRenderGraphState::CloneAssumingInert() const {
    TemporalRenderGraphState new_state;
    for (const auto& [key, state] : resources_) {
        new_state.resources_.emplace(key, state.CloneAssumingInert());
    }
    return new_state;
}

void TemporalRenderGraphState::AddResource(const TemporalResourceKey& key, TemporalResourceState state) {
    resources_.emplace(key, std::move(state));
}

const TemporalResourceState* TemporalRenderGraphState::GetResource(const TemporalResourceKey& key) const {
    auto it = resources_.find(key);
    return it != resources_.end() ? &it->second : nullptr;
}

TemporalResourceState* TemporalRenderGraphState::GetResourceMut(const TemporalResourceKey& key) {
    auto it = resources_.find(key);
    return it != resources_.end() ? &it->second : nullptr;
}

TemporalRenderGraph::TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device)
    : rg_(), device_(std::move(device)), temporal_state_(std::move(state)) {}

Handle<Image> TemporalRenderGraph::GetOrCreateTemporalImage(const TemporalResourceKey& key, const ImageDesc& desc) {
    auto it = temporal_state_.resources_.find(key);
    if (it != temporal_state_.resources_.end()) {
        TemporalResourceState& state = it->second;
        
        if (state.GetType() == TemporalResourceStateType::Inert) {
            const auto& inert_state = state.GetInertState();
            const auto& resource = inert_state.resource;
            
            if (resource.GetType() != TemporalResourceType::Image) {
                throw std::runtime_error("Resource is a buffer, but an image was requested");
            }
            
            auto image = resource.GetImage();
            auto handle = rg_.Import(image, inert_state.access_type);
            
            state = TemporalResourceState(TemporalResourceState::ImportedState{
                resource.Clone(),
                ExportableGraphResource(handle.CloneUnchecked())
            });
            
            return handle;
        } else if (state.GetType() == TemporalResourceStateType::Imported) {
            throw std::runtime_error("Temporal resource already taken");
        } else {
            throw std::runtime_error("Unexpected exported state");
        }
    } else {
        try {
            auto resource = std::make_shared<Image>(device_->CreateImage(desc, std::vector<uint8_t>{}));
            auto handle = rg_.Import(resource, AccessType::Nothing);
            
            temporal_state_.AddResource(key, TemporalResourceState(TemporalResourceState::ImportedState{
                TemporalResource(resource),
                ExportableGraphResource(handle.CloneUnchecked())
            }));
            
            return handle;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Creating image failed: ") + e.what());
        }
    }
}

Handle<Buffer> TemporalRenderGraph::GetOrCreateTemporalBuffer(const TemporalResourceKey& key, const BufferDesc& desc) {
    auto it = temporal_state_.resources_.find(key);
    if (it != temporal_state_.resources_.end()) {
        TemporalResourceState& state = it->second;
        
        if (state.GetType() == TemporalResourceStateType::Inert) {
            const auto& inert_state = state.GetInertState();
            const auto& resource = inert_state.resource;
            
            if (resource.GetType() != TemporalResourceType::Buffer) {
                throw std::runtime_error("Resource is an image, but a buffer was requested");
            }
            
            auto buffer = resource.GetBuffer();
            auto handle = rg_.Import(buffer, inert_state.access_type);
            
            state = TemporalResourceState(TemporalResourceState::ImportedState{
                resource.Clone(),
                ExportableGraphResource(handle.CloneUnchecked())
            });
            
            return handle;
        } else if (state.GetType() == TemporalResourceStateType::Imported) {
            throw std::runtime_error("Temporal resource already taken");
        } else {
            throw std::runtime_error("Unexpected exported state");
        }
    } else {
        try {
            std::vector<uint8_t> zero_init(desc.size, 0);
            auto resource = std::make_shared<Buffer>(device_->CreateBuffer(desc, zero_init));
            auto handle = rg_.Import(resource, AccessType::Nothing);
            
            temporal_state_.AddResource(key, TemporalResourceState(TemporalResourceState::ImportedState{
                TemporalResource(resource),
                ExportableGraphResource(handle.CloneUnchecked())
            }));
            
            return handle;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Creating buffer failed: ") + e.what());
        }
    }
}

std::pair<RenderGraph, ExportedTemporalRenderGraphState> TemporalRenderGraph::ExportTemporal() {
    RenderGraph rg = std::move(rg_);
    
    for (auto& [key, state] : temporal_state_.resources_) {
        if (state.GetType() == TemporalResourceStateType::Imported) {
            const auto& imported_state = state.GetImportedState();
            const auto& handle = imported_state.handle;
            
            if (handle.GetType() == ExportableGraphResourceType::Image) {
                auto exported_handle = rg.Export(handle.GetImageHandle().CloneUnchecked(), AccessType::Nothing);
                state = TemporalResourceState(TemporalResourceState::ExportedState{
                    imported_state.resource.Clone(),
                    ExportedResourceHandle(exported_handle)
                });
            } else if (handle.GetType() == ExportableGraphResourceType::Buffer) {
                auto exported_handle = rg.Export(handle.GetBufferHandle().CloneUnchecked(), AccessType::Nothing);
                state = TemporalResourceState(TemporalResourceState::ExportedState{
                    imported_state.resource.Clone(),
                    ExportedResourceHandle(exported_handle)
                });
            }
        } else if (state.GetType() == TemporalResourceStateType::Exported) {
            throw std::runtime_error("Unexpected exported state during export");
        }
    }
    
    return {std::move(rg), ExportedTemporalRenderGraphState(std::move(temporal_state_))};
}

TemporalRenderGraphState ExportedTemporalRenderGraphState::RetireTemporal(const RetiredRenderGraph& rg) {
    TemporalRenderGraphState state = std::move(state_);
    
    for (auto& [key, resource_state] : state.resources_) {
        if (resource_state.GetType() == TemporalResourceStateType::Exported) {
            const auto& exported_state = resource_state.GetExportedState();
            const auto& handle = exported_state.handle;
            
            if (handle.GetType() == ExportedResourceHandleType::Image) {
                auto exported_resource = rg.GetExportedResource(handle.GetImageHandle());
                resource_state = TemporalResourceState(TemporalResourceState::InertState{
                    exported_state.resource.Clone(),
                    exported_resource.second
                });
            } else if (handle.GetType() == ExportedResourceHandleType::Buffer) {
                auto exported_resource = rg.GetExportedResource(handle.GetBufferHandle());
                resource_state = TemporalResourceState(TemporalResourceState::InertState{
                    exported_state.resource.Clone(),
                    exported_resource.second
                });
            }
        } else if (resource_state.GetType() == TemporalResourceStateType::Imported) {
            throw std::runtime_error("Unexpected imported state during retire");
        }
    }
    
    return state;
}

} // namespace tekki::render_graph