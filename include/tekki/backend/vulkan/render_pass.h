#pragma once

#include "device.h"

#include <vulkan/vulkan.hpp>

namespace tekki::vulkan {

struct RenderPassAttachmentDesc {
    vk::Format format;
    vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout final_layout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::AttachmentLoadOp load_op = vk::AttachmentLoadOp::eDontCare;
    vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore;
    vk::ClearValue clear_value;

    RenderPassAttachmentDesc() = default;
    explicit RenderPassAttachmentDesc(vk::Format format) : format(format) {}

    RenderPassAttachmentDesc& garbage_input() {
        load_op = vk::AttachmentLoadOp::eDontCare;
        return *this;
    }

    static RenderPassAttachmentDesc new_(vk::Format format) {
        return RenderPassAttachmentDesc(format);
    }
};

struct RenderPassDesc {
    std::vector<RenderPassAttachmentDesc> color_attachments;
    std::optional<RenderPassAttachmentDesc> depth_attachment;
};

class RenderPass {
public:
    RenderPass(std::shared_ptr<Device> device, const RenderPassDesc& desc);
    ~RenderPass();

    vk::RenderPass raw() const { return render_pass_; }

private:
    std::shared_ptr<Device> device_;
    vk::RenderPass render_pass_;
};

} // namespace tekki::vulkan