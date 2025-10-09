#include "tekki/render_graph/temporal.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/render_graph/graph.h"
#include <stdexcept>

namespace tekki::render_graph {

// TemporalRenderGraph constructor
TemporalRenderGraph::TemporalRenderGraph(TemporalRenderGraphState state, std::shared_ptr<Device> device)
    : rg(), device_(std::move(device)), temporal_state(std::move(state)) {}

// Export temporal state
std::pair<RenderGraph, ExportedTemporalRenderGraphState> TemporalRenderGraph::export_temporal()
{
    auto moved_rg = std::move(rg);
    auto moved_state = std::move(temporal_state);

    // Convert imported states to exported states
    for (auto& [key, state] : moved_state.resources)
    {
        if (state.type == TemporalResourceState::Type::Imported)
        {
            auto& [resource, handle] = std::get<1>(state.data);

            if (std::holds_alternative<ExportableGraphResource>(handle))
            {
                auto& exportable = std::get<ExportableGraphResource>(handle);

                if (std::holds_alternative<Handle<ImageResource>>(exportable.data))
                {
                    auto img_handle = std::get<Handle<ImageResource>>(exportable.data);
                    auto exported_handle = moved_rg.Export(img_handle, backend::vulkan::AccessType::Nothing);

                    ExportedResourceHandle exported_res_handle{
                        .type = ExportedResourceHandle::Type::Image,
                        .data = exported_handle
                    };

                    state = TemporalResourceState{
                        .type = TemporalResourceState::Type::Exported,
                        .data = std::make_tuple(resource, exported_res_handle)
                    };
                }
                else if (std::holds_alternative<Handle<BufferResource>>(exportable.data))
                {
                    auto buf_handle = std::get<Handle<BufferResource>>(exportable.data);
                    auto exported_handle = moved_rg.Export(buf_handle, backend::vulkan::AccessType::Nothing);

                    ExportedResourceHandle exported_res_handle{
                        .type = ExportedResourceHandle::Type::Buffer,
                        .data = exported_handle
                    };

                    state = TemporalResourceState{
                        .type = TemporalResourceState::Type::Exported,
                        .data = std::make_tuple(resource, exported_res_handle)
                    };
                }
            }
        }
    }

    return {std::move(moved_rg), ExportedTemporalRenderGraphState{std::move(moved_state)}};
}

// Retire temporal state
TemporalRenderGraphState ExportedTemporalRenderGraphState::retire_temporal(const RetiredRenderGraph& rg) const
{
    TemporalRenderGraphState result_state = state;

    for (auto& [key, state_ref] : result_state.resources)
    {
        if (state_ref.type == TemporalResourceState::Type::Exported)
        {
            auto& [resource, handle] = std::get<2>(state_ref.data);

            backend::vulkan::AccessType access_type;

            if (handle.type == ExportedResourceHandle::Type::Image)
            {
                auto img_handle = std::get<ExportedHandle<ImageResource>>(handle.data);
                auto [_, exported_access] = rg.ExportedResource(img_handle);
                access_type = exported_access;
            }
            else // Buffer
            {
                auto buf_handle = std::get<ExportedHandle<BufferResource>>(handle.data);
                auto [_, exported_access] = rg.ExportedResource(buf_handle);
                access_type = exported_access;
            }

            state_ref = TemporalResourceState{
                .type = TemporalResourceState::Type::Inert,
                .data = std::make_tuple(resource, access_type)
            };
        }
    }

    return result_state;
}

// Specialization for ImageDesc
Handle<ImageResource> GetOrCreateTemporal<backend::vulkan::ImageDesc>::get_or_create_temporal(
    TemporalRenderGraph& self, const TemporalResourceKey& key, const backend::vulkan::ImageDesc& desc)
{
    auto it = self.temporal_state.resources.find(key);

    if (it != self.temporal_state.resources.end())
    {
        // Resource exists
        auto& state = it->second;

        if (state.type == TemporalResourceState::Type::Inert)
        {
            auto& [resource, access_type] = std::get<0>(state.data);

            if (resource.type == TemporalResource::Type::Image)
            {
                auto image_ptr = std::get<std::shared_ptr<Image>>(resource.data);
                auto handle = self.rg.Import(image_ptr, access_type);

                ExportableGraphResource exportable{
                    .type = ExportableGraphResource::Type::Image,
                    .data = handle.clone_unchecked()
                };

                state = TemporalResourceState{
                    .type = TemporalResourceState::Type::Imported,
                    .data = std::make_tuple(resource, exportable)
                };

                return handle;
            }
            else
            {
                throw std::runtime_error("Resource is a buffer, but an image was requested");
            }
        }
        else
        {
            throw std::runtime_error("Temporal resource already taken");
        }
    }
    else
    {
        // Create new resource
        auto image = std::make_shared<Image>(self.device()->CreateImage(desc, std::vector<uint8_t>{}));
        auto handle = self.rg.Import(image, backend::vulkan::AccessType::Nothing);

        TemporalResource temporal_res{
            .type = TemporalResource::Type::Image,
            .data = image
        };

        ExportableGraphResource exportable{
            .type = ExportableGraphResource::Type::Image,
            .data = handle.clone_unchecked()
        };

        TemporalResourceState temporal_state{
            .type = TemporalResourceState::Type::Imported,
            .data = std::make_tuple(temporal_res, exportable)
        };

        self.temporal_state.resources[key] = temporal_state;

        return handle;
    }
}

// Specialization for BufferDesc
Handle<BufferResource> GetOrCreateTemporal<backend::vulkan::BufferDesc>::get_or_create_temporal(
    TemporalRenderGraph& self, const TemporalResourceKey& key, const backend::vulkan::BufferDesc& desc)
{
    auto it = self.temporal_state.resources.find(key);

    if (it != self.temporal_state.resources.end())
    {
        // Resource exists
        auto& state = it->second;

        if (state.type == TemporalResourceState::Type::Inert)
        {
            auto& [resource, access_type] = std::get<0>(state.data);

            if (resource.type == TemporalResource::Type::Buffer)
            {
                auto buffer_ptr = std::get<std::shared_ptr<Buffer>>(resource.data);
                auto handle = self.rg.Import(buffer_ptr, access_type);

                ExportableGraphResource exportable{
                    .type = ExportableGraphResource::Type::Buffer,
                    .data = handle.clone_unchecked()
                };

                state = TemporalResourceState{
                    .type = TemporalResourceState::Type::Imported,
                    .data = std::make_tuple(resource, exportable)
                };

                return handle;
            }
            else
            {
                throw std::runtime_error("Resource is an image, but a buffer was requested");
            }
        }
        else
        {
            throw std::runtime_error("Temporal resource already taken");
        }
    }
    else
    {
        // Create new buffer - zero initialized
        std::vector<uint8_t> zero_data(desc.size, 0);
        auto buffer = std::make_shared<Buffer>(self.device()->CreateBuffer(desc, key.name, &zero_data));
        auto handle = self.rg.Import(buffer, backend::vulkan::AccessType::Nothing);

        TemporalResource temporal_res{
            .type = TemporalResource::Type::Buffer,
            .data = buffer
        };

        ExportableGraphResource exportable{
            .type = ExportableGraphResource::Type::Buffer,
            .data = handle.clone_unchecked()
        };

        TemporalResourceState temporal_state{
            .type = TemporalResourceState::Type::Imported,
            .data = std::make_tuple(temporal_res, exportable)
        };

        self.temporal_state.resources[key] = temporal_state;

        return handle;
    }
}

} // namespace tekki::render_graph
