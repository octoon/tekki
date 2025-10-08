#include "tekki/render_graph/temporal.h"

namespace tekki {
namespace render_graph {

// TemporalRenderGraph constructor
TemporalRenderGraph::TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device)
    : rg(), device_(std::move(device)), temporal_state(std::move(state)) {}

// Temporal resource management
template<typename Desc>
Handle<typename Desc::ResourceType> TemporalRenderGraph::get_or_create_temporal(
    const TemporalResourceKey& key,
    const Desc& desc) {

    return GetOrCreateTemporal<Desc>::get_or_create_temporal(*this, key, desc);
}

// Export temporal state
std::pair<RenderGraph, ExportedTemporalRenderGraphState> TemporalRenderGraph::export_temporal() {
    ExportedTemporalRenderGraphState exported_state{
        .state = std::move(temporal_state)
    };

    return {std::move(rg), std::move(exported_state)};
}

// GetOrCreateTemporal specializations
Handle<ImageResource> GetOrCreateTemporal<ImageDesc>::get_or_create_temporal(
    TemporalRenderGraph& self,
    const TemporalResourceKey& key,
    const ImageDesc& desc) {

    auto it = self.temporal_state.resources.find(key);
    if (it != self.temporal_state.resources.end()) {
        auto& state = it->second;

        switch (state.type) {
            case TemporalResourceState::Type::Inert: {
                auto& inert_data = std::get<0>(state.data);
                auto image = std::get<std::shared_ptr<Image>>(inert_data.resource.data);
                return self.rg->import(image, inert_data.access_type);
            }
            case TemporalResourceState::Type::Imported: {
                auto& imported_data = std::get<1>(state.data);
                auto image = std::get<std::shared_ptr<Image>>(imported_data.resource.data);
                return self.rg->import(image, backend::vulkan::AccessType::Nothing);
            }
            case TemporalResourceState::Type::Exported: {
                auto& exported_data = std::get<2>(state.data);
                auto image = std::get<std::shared_ptr<Image>>(exported_data.resource.data);
                return self.rg->import(image, backend::vulkan::AccessType::Nothing);
            }
        }
    }

    // Create new temporal resource
    auto image = std::make_shared<Image>(self.device()->create_image(desc));
    auto handle = self.rg->import(image, backend::vulkan::AccessType::Nothing);

    self.temporal_state.resources[key] = TemporalResourceState{
        .type = TemporalResourceState::Type::Imported,
        .data = TemporalResourceState::Imported{
            .resource = TemporalResource{
                .type = TemporalResource::Type::Image,
                .data = image
            },
            .handle = ExportableGraphResource{
                .type = ExportableGraphResource::Type::Image,
                .data = handle
            }
        }
    };

    return handle;
}

Handle<BufferResource> GetOrCreateTemporal<BufferDesc>::get_or_create_temporal(
    TemporalRenderGraph& self,
    const TemporalResourceKey& key,
    const BufferDesc& desc) {

    auto it = self.temporal_state.resources.find(key);
    if (it != self.temporal_state.resources.end()) {
        auto& state = it->second;

        switch (state.type) {
            case TemporalResourceState::Type::Inert: {
                auto& inert_data = std::get<0>(state.data);
                auto buffer = std::get<std::shared_ptr<Buffer>>(inert_data.resource.data);
                return self.rg->import(buffer, inert_data.access_type);
            }
            case TemporalResourceState::Type::Imported: {
                auto& imported_data = std::get<1>(state.data);
                auto buffer = std::get<std::shared_ptr<Buffer>>(imported_data.resource.data);
                return self.rg->import(buffer, backend::vulkan::AccessType::Nothing);
            }
            case TemporalResourceState::Type::Exported: {
                auto& exported_data = std::get<2>(state.data);
                auto buffer = std::get<std::shared_ptr<Buffer>>(exported_data.resource.data);
                return self.rg->import(buffer, backend::vulkan::AccessType::Nothing);
            }
        }
    }

    // Create new temporal resource
    auto buffer = std::make_shared<Buffer>(self.device()->create_buffer(desc));
    auto handle = self.rg->import(buffer, backend::vulkan::AccessType::Nothing);

    self.temporal_state.resources[key] = TemporalResourceState{
        .type = TemporalResourceState::Type::Imported,
        .data = TemporalResourceState::Imported{
            .resource = TemporalResource{
                .type = TemporalResource::Type::Buffer,
                .data = buffer
            },
            .handle = ExportableGraphResource{
                .type = ExportableGraphResource::Type::Buffer,
                .data = handle
            }
        }
    };

    return handle;
}

// Retire temporal state
TemporalRenderGraphState retire_temporal(
    const ExportedTemporalRenderGraphState& exported_state,
    const RetiredRenderGraph& rg) {

    TemporalRenderGraphState result;

    for (const auto& [key, state] : exported_state.state.resources) {
        switch (state.type) {
            case TemporalResourceState::Type::Inert: {
                // Keep inert resources as-is
                result.resources[key] = state;
                break;
            }
            case TemporalResourceState::Type::Imported: {
                // Convert imported resources to inert
                auto& imported_data = std::get<1>(state.data);
                result.resources[key] = TemporalResourceState{
                    .type = TemporalResourceState::Type::Inert,
                    .data = TemporalResourceState::Inert{
                        .resource = imported_data.resource.clone(),
                        .access_type = backend::vulkan::AccessType::Nothing
                    }
                };
                break;
            }
            case TemporalResourceState::Type::Exported: {
                // Convert exported resources to inert
                auto& exported_data = std::get<2>(state.data);
                result.resources[key] = TemporalRenderGraphState{
                    .type = TemporalResourceState::Type::Inert,
                    .data = TemporalResourceState::Inert{
                        .resource = exported_data.resource.clone(),
                        .access_type = backend::vulkan::AccessType::Nothing
                    }
                };
                break;
            }
        }
    }

    return result;
}

// Explicit template instantiations
template Handle<ImageResource> TemporalRenderGraph::get_or_create_temporal<ImageDesc>(const TemporalResourceKey&, const ImageDesc&);
template Handle<BufferResource> TemporalRenderGraph::get_or_create_temporal<BufferDesc>(const TemporalResourceKey&, const BufferDesc&);

} // namespace render_graph
} // namespace tekki