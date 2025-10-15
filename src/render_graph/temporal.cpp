#include "tekki/render_graph/temporal.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <utility>
#include "tekki/core/result.h"
#include "tekki/backend/vk_sync.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/render_graph/graph.h"
#include "tekki/render_graph/resource.h"
#include "tekki/render_graph/buffer.h"
#include "tekki/render_graph/Image.h"

namespace tekki::render_graph {

// All inline methods are defined in the header file
// Only non-inline implementations go here

TemporalRenderGraph::TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device)
    : rg_(std::make_unique<RenderGraph>()), device_(std::move(device)), temporal_state_(std::move(state)) {}

Handle<Image> TemporalRenderGraph::GetOrCreateTemporalImage(const TemporalResourceKey& key, const ImageDesc& desc) {
    TemporalResourceState* statePtr = temporal_state_.GetResourceMut(key);
    if (statePtr) {
        TemporalResourceState& state = *statePtr;

        if (state.GetType() == TemporalResourceStateType::Inert) {
            const auto& inert_state = state.GetInertState();
            const auto& resource = inert_state.resource;

            if (resource.GetType() != TemporalResourceType::Image) {
                throw std::runtime_error("Resource is a buffer, but an image was requested");
            }

            auto image = resource.GetImage();
            auto handle = rg_->Import(image, inert_state.access_type);

            state = TemporalResourceState(TemporalResourceState::ImportedState{
                resource.Clone(),
                std::make_shared<ExportableGraphResource>(handle)
            });

            return handle;
        } else if (state.GetType() == TemporalResourceStateType::Imported) {
            throw std::runtime_error("Temporal resource already taken");
        } else {
            throw std::runtime_error("Unexpected exported state");
        }
    } else {
        try {
            std::shared_ptr<tekki::backend::vulkan::Image> vkImage = device_->CreateImage(desc, std::vector<uint8_t>{});

            // Create render_graph::Image wrapper
            std::shared_ptr<Image> resource = std::make_shared<Image>(vkImage->Raw, desc);

            auto handle = rg_->Import(resource, vk_sync::AccessType::Nothing);

            temporal_state_.AddResource(key, TemporalResourceState(TemporalResourceState::ImportedState{
                TemporalResource(resource),
                std::make_shared<ExportableGraphResource>(handle)
            }));

            return handle;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Creating image failed: ") + e.what());
        }
    }
}

Handle<Buffer> TemporalRenderGraph::GetOrCreateTemporalBuffer(const TemporalResourceKey& key, const BufferDesc& desc) {
    TemporalResourceState* statePtr = temporal_state_.GetResourceMut(key);
    if (statePtr) {
        TemporalResourceState& state = *statePtr;

        if (state.GetType() == TemporalResourceStateType::Inert) {
            const auto& inert_state = state.GetInertState();
            const auto& resource = inert_state.resource;

            if (resource.GetType() != TemporalResourceType::Buffer) {
                throw std::runtime_error("Resource is an image, but a buffer was requested");
            }

            auto buffer = resource.GetBuffer();
            auto handle = rg_->Import(buffer, inert_state.access_type);

            state = TemporalResourceState(TemporalResourceState::ImportedState{
                resource.Clone(),
                std::make_shared<ExportableGraphResource>(handle)
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
            tekki::backend::vulkan::Buffer vkBuffer = device_->CreateBuffer(desc, "temporal buffer", zero_init);

            // Create render_graph::Buffer wrapper
            std::shared_ptr<Buffer> resource = std::make_shared<Buffer>(vkBuffer.Raw, desc, std::move(vkBuffer.Allocation));

            auto handle = rg_->Import(resource, vk_sync::AccessType::Nothing);

            temporal_state_.AddResource(key, TemporalResourceState(TemporalResourceState::ImportedState{
                TemporalResource(resource),
                std::make_shared<ExportableGraphResource>(handle)
            }));

            return handle;
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Creating buffer failed: ") + e.what());
        }
    }
}

std::pair<std::unique_ptr<RenderGraph>, ExportedTemporalRenderGraphState> TemporalRenderGraph::ExportTemporal() {
    auto rg = std::move(rg_);

    for (auto it = temporal_state_.begin(); it != temporal_state_.end(); ++it) {
        auto& [key, state] = *it;
        if (state.GetType() == TemporalResourceStateType::Imported) {
            const auto& imported_state = state.GetImportedState();
            const auto& handle = imported_state.handle;

            if (handle->GetType() == ExportableGraphResource::Type::Image) {
                auto exported_handle = rg->Export(handle->GetImageHandle(), vk_sync::AccessType::Nothing);
                state = TemporalResourceState(TemporalResourceState::ExportedState{
                    imported_state.resource.Clone(),
                    ExportedResourceHandle(exported_handle)
                });
            } else if (handle->GetType() == ExportableGraphResource::Type::Buffer) {
                auto exported_handle = rg->Export(handle->GetBufferHandle(), vk_sync::AccessType::Nothing);
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

    for (auto it = state.begin(); it != state.end(); ++it) {
        auto& [key, resource_state] = *it;
        if (resource_state.GetType() == TemporalResourceStateType::Exported) {
            const auto& exported_state = resource_state.GetExportedState();
            const auto& handle = exported_state.handle;

            if (handle.GetType() == ExportedResourceHandleType::Image) {
                auto exported_resource = rg.ExportedResource(handle.GetImageHandle());
                resource_state = TemporalResourceState(TemporalResourceState::InertState{
                    exported_state.resource.Clone(),
                    exported_resource.second
                });
            } else if (handle.GetType() == ExportedResourceHandleType::Buffer) {
                auto exported_resource = rg.ExportedResource(handle.GetBufferHandle());
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
