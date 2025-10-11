#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <array>
#include <optional>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/image.h"

namespace tekki::backend::vulkan {

// Forward declarations
class Device;
class RenderPass;

// SPIRV reflection types (replacing rspirv_reflect)
enum class DescriptorType {
    SAMPLER,
    COMBINED_IMAGE_SAMPLER,
    SAMPLED_IMAGE,
    STORAGE_IMAGE,
    UNIFORM_TEXEL_BUFFER,
    STORAGE_TEXEL_BUFFER,
    UNIFORM_BUFFER,
    STORAGE_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER_DYNAMIC,
    INPUT_ATTACHMENT,
    ACCELERATION_STRUCTURE_KHR
};

enum class DescriptorDimensionality {
    Single,
    RuntimeArray
};

struct DescriptorInfo {
    DescriptorType ty;
    DescriptorDimensionality dimensionality;
    std::string name;
};

constexpr size_t MAX_DESCRIPTOR_SETS = 4;
constexpr size_t MAX_COLOR_ATTACHMENTS = 8;

using DescriptorSetLayout = std::unordered_map<uint32_t, DescriptorInfo>;
using StageDescriptorSetLayouts = std::unordered_map<uint32_t, DescriptorSetLayout>;

struct ShaderPipelineCommon {
    VkPipelineLayout PipelineLayout;
    VkPipeline Pipeline;
    std::vector<std::unordered_map<uint32_t, VkDescriptorType>> SetLayoutInfo;
    std::vector<VkDescriptorPoolSize> DescriptorPoolSizes;
    std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
    VkPipelineBindPoint PipelineBindPoint;
};

class ComputePipeline {
public:
    ShaderPipelineCommon Common;
    std::array<uint32_t, 3> GroupSize;

    const ShaderPipelineCommon& operator*() const { return Common; }
    const ShaderPipelineCommon* operator->() const { return &Common; }
};

class RasterPipeline {
public:
    ShaderPipelineCommon Common;

    const ShaderPipelineCommon& operator*() const { return Common; }
    const ShaderPipelineCommon* operator->() const { return &Common; }
};

struct DescriptorSetLayoutOpts {
    std::optional<VkDescriptorSetLayoutCreateFlags> Flags;
    std::optional<DescriptorSetLayout> Replace;

    class Builder {
    private:
        std::unique_ptr<DescriptorSetLayoutOpts> Options;

    public:
        Builder() : Options(std::make_unique<DescriptorSetLayoutOpts>()) {}

        Builder& SetFlags(std::optional<VkDescriptorSetLayoutCreateFlags> flags) {
            Options->Flags = flags;
            return *this;
        }

        Builder& SetReplace(std::optional<DescriptorSetLayout> replace) {
            Options->Replace = replace;
            return *this;
        }

        DescriptorSetLayoutOpts Build() const {
            return *Options;
        }
    };

    static Builder CreateBuilder() {
        return Builder();
    }
};

enum class ShaderSourceType {
    Rust,
    Hlsl
};

class ShaderSource {
private:
    ShaderSourceType Type;
    std::string RustEntry;
    std::filesystem::path HlslPath;

public:
    static ShaderSource CreateRust(const std::string& entry) {
        ShaderSource source;
        source.Type = ShaderSourceType::Rust;
        source.RustEntry = entry;
        return source;
    }

    static ShaderSource CreateHlsl(const std::filesystem::path& path) {
        ShaderSource source;
        source.Type = ShaderSourceType::Hlsl;
        source.HlslPath = path;
        return source;
    }

    std::string GetEntry() const {
        if (Type == ShaderSourceType::Rust) {
            return RustEntry;
        } else {
            return "main";
        }
    }

    ShaderSourceType GetType() const { return Type; }
    const std::string& GetRustEntry() const { return RustEntry; }
    const std::filesystem::path& GetHlslPath() const { return HlslPath; }

    bool operator==(const ShaderSource& other) const {
        if (Type != other.Type) return false;
        if (Type == ShaderSourceType::Rust) {
            return RustEntry == other.RustEntry;
        } else {
            return HlslPath == other.HlslPath;
        }
    }

    bool operator!=(const ShaderSource& other) const {
        return !(*this == other);
    }
};

struct ComputePipelineDesc {
    std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> DescriptorSetOpts;
    size_t PushConstantsBytes = 0;
    ShaderSource Source;

    class Builder {
    private:
        std::unique_ptr<ComputePipelineDesc> Desc;

    public:
        Builder() : Desc(std::make_unique<ComputePipelineDesc>()) {}

        Builder& SetDescriptorSetOpts(const std::vector<std::pair<uint32_t, DescriptorSetLayoutOpts::Builder>>& opts) {
            if (opts.size() > MAX_DESCRIPTOR_SETS) {
                throw std::runtime_error("Too many descriptor set options");
            }

            for (size_t i = 0; i < opts.size(); ++i) {
                Desc->DescriptorSetOpts[i] = std::make_pair(opts[i].first, opts[i].second.Build());
            }
            return *this;
        }

        Builder& SetPushConstantsBytes(size_t bytes) {
            Desc->PushConstantsBytes = bytes;
            return *this;
        }

        Builder& SetSource(const ShaderSource& source) {
            Desc->Source = source;
            return *this;
        }

        Builder& SetComputeRust(const std::string& entry) {
            Desc->Source = ShaderSource::CreateRust(entry);
            return *this;
        }

        Builder& SetComputeHlsl(const std::filesystem::path& path) {
            Desc->Source = ShaderSource::CreateHlsl(path);
            return *this;
        }

        ComputePipelineDesc Build() const {
            return *Desc;
        }
    };

    static Builder CreateBuilder() {
        return Builder();
    }
};

enum class ShaderPipelineStage {
    Vertex,
    Pixel,
    RayGen,
    RayMiss,
    RayClosestHit
};

struct PipelineShaderDesc {
    ShaderPipelineStage Stage;
    std::optional<std::vector<std::pair<size_t, VkDescriptorSetLayoutCreateFlags>>> DescriptorSetLayoutFlags;
    size_t PushConstantsBytes = 0;
    std::string Entry = "main";
    ShaderSource Source;

    class Builder {
    private:
        std::unique_ptr<PipelineShaderDesc> Desc;

    public:
        Builder(ShaderPipelineStage stage) : Desc(std::make_unique<PipelineShaderDesc>()) {
            Desc->Stage = stage;
            Desc->Entry = "main";
        }

        Builder& SetDescriptorSetLayoutFlags(std::optional<std::vector<std::pair<size_t, VkDescriptorSetLayoutCreateFlags>>> flags) {
            Desc->DescriptorSetLayoutFlags = flags;
            return *this;
        }

        Builder& SetPushConstantsBytes(size_t bytes) {
            Desc->PushConstantsBytes = bytes;
            return *this;
        }

        Builder& SetEntry(const std::string& entry) {
            Desc->Entry = entry;
            return *this;
        }

        Builder& SetSource(const ShaderSource& source) {
            Desc->Source = source;
            return *this;
        }

        Builder& SetHlslSource(const std::filesystem::path& path) {
            Desc->Source = ShaderSource::CreateHlsl(path);
            return *this;
        }

        Builder& SetRustSource(const std::string& entry) {
            Desc->Source = ShaderSource::CreateRust(entry);
            return *this;
        }

        PipelineShaderDesc Build() const {
            return *Desc;
        }
    };

    static Builder CreateBuilder(ShaderPipelineStage stage) {
        return Builder(stage);
    }

    bool operator==(const PipelineShaderDesc& other) const {
        return Stage == other.Stage &&
               DescriptorSetLayoutFlags == other.DescriptorSetLayoutFlags &&
               PushConstantsBytes == other.PushConstantsBytes &&
               Entry == other.Entry &&
               Source == other.Source;
    }

    bool operator!=(const PipelineShaderDesc& other) const {
        return !(*this == other);
    }
};

struct RasterPipelineDesc {
    std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> DescriptorSetOpts;
    std::shared_ptr<RenderPass> RenderPassPtr;
    bool FaceCull = false;
    bool DepthWrite = false;
    size_t PushConstantsBytes = 0;

    class Builder {
    private:
        std::unique_ptr<RasterPipelineDesc> Desc;

    public:
        Builder() : Desc(std::make_unique<RasterPipelineDesc>()) {}

        Builder& SetDescriptorSetOpts(const std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS>& opts) {
            Desc->DescriptorSetOpts = opts;
            return *this;
        }

        Builder& SetRenderPass(std::shared_ptr<RenderPass> renderPass) {
            Desc->RenderPassPtr = renderPass;
            return *this;
        }

        Builder& SetFaceCull(bool faceCull) {
            Desc->FaceCull = faceCull;
            return *this;
        }

        Builder& SetDepthWrite(bool depthWrite) {
            Desc->DepthWrite = depthWrite;
            return *this;
        }

        Builder& SetPushConstantsBytes(size_t bytes) {
            Desc->PushConstantsBytes = bytes;
            return *this;
        }

        RasterPipelineDesc Build() const {
            return *Desc;
        }
    };

    static Builder CreateBuilder() {
        return Builder();
    }
};

struct RenderPassAttachmentDesc {
    VkFormat Format;
    VkAttachmentLoadOp LoadOp;
    VkAttachmentStoreOp StoreOp;
    VkSampleCountFlagBits Samples;

    RenderPassAttachmentDesc(VkFormat format)
        : Format(format)
        , LoadOp(VK_ATTACHMENT_LOAD_OP_LOAD)
        , StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
        , Samples(VK_SAMPLE_COUNT_1_BIT) {}

    RenderPassAttachmentDesc& SetGarbageInput() {
        LoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        return *this;
    }

    RenderPassAttachmentDesc& SetClearInput() {
        LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        return *this;
    }

    RenderPassAttachmentDesc& SetDiscardOutput() {
        StoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        return *this;
    }

    VkAttachmentDescription ToVk(VkImageLayout initialLayout, VkImageLayout finalLayout) const {
        return VkAttachmentDescription{
            .format = Format,
            .samples = Samples,
            .loadOp = LoadOp,
            .storeOp = StoreOp,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = initialLayout,
            .finalLayout = finalLayout
        };
    }
};

struct FramebufferCacheKey {
    std::array<uint32_t, 2> Dims;
    std::vector<std::pair<VkImageUsageFlags, VkImageCreateFlags>> Attachments;

    bool operator==(const FramebufferCacheKey& other) const {
        return Dims == other.Dims && Attachments == other.Attachments;
    }

    static FramebufferCacheKey Create(
        const std::array<uint32_t, 2>& dims,
        const std::vector<ImageDesc>& colorAttachments,
        const std::optional<ImageDesc>& depthStencilAttachment) {
        
        std::vector<std::pair<VkImageUsageFlags, VkImageCreateFlags>> attachments;
        attachments.reserve(colorAttachments.size() + (depthStencilAttachment.has_value() ? 1 : 0));
        
        for (const auto& attachment : colorAttachments) {
            attachments.emplace_back(attachment.Usage, attachment.Flags);
        }
        
        if (depthStencilAttachment) {
            attachments.emplace_back(depthStencilAttachment->Usage, depthStencilAttachment->Flags);
        }
        
        return FramebufferCacheKey{dims, std::move(attachments)};
    }
};

struct FramebufferCacheKeyHash {
    std::size_t operator()(const FramebufferCacheKey& key) const {
        std::size_t seed = 0;
        seed ^= std::hash<uint32_t>{}(key.Dims[0]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint32_t>{}(key.Dims[1]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        for (const auto& attachment : key.Attachments) {
            seed ^= std::hash<VkImageUsageFlags>{}(attachment.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<VkImageCreateFlags>{}(attachment.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

class FramebufferCache {
private:
    std::unordered_map<FramebufferCacheKey, VkFramebuffer, FramebufferCacheKeyHash> Entries;
    std::vector<RenderPassAttachmentDesc> AttachmentDesc;
    VkRenderPass RenderPass;
    size_t ColorAttachmentCount;

public:
    FramebufferCache(VkRenderPass renderPass,
                    const std::vector<RenderPassAttachmentDesc>& colorAttachments,
                    const std::optional<RenderPassAttachmentDesc>& depthAttachment)
        : RenderPass(renderPass)
        , ColorAttachmentCount(colorAttachments.size()) {
        
        AttachmentDesc = colorAttachments;
        if (depthAttachment) {
            AttachmentDesc.push_back(*depthAttachment);
        }
    }

    VkFramebuffer GetOrCreate(VkDevice device, const FramebufferCacheKey& key) {
        auto it = Entries.find(key);
        if (it != Entries.end()) {
            return it->second;
        }

        std::vector<VkFormat> colorFormats;
        colorFormats.reserve(AttachmentDesc.size());
        for (const auto& desc : AttachmentDesc) {
            colorFormats.push_back(desc.Format);
        }

        std::vector<VkFramebufferAttachmentImageInfo> attachmentImageInfos;
        attachmentImageInfos.reserve(AttachmentDesc.size());
        
        for (size_t i = 0; i < AttachmentDesc.size(); ++i) {
            const auto& attachment = key.Attachments[i];

            attachmentImageInfos.push_back(VkFramebufferAttachmentImageInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                .pNext = nullptr,
                .flags = attachment.second,
                .usage = attachment.first,
                .width = key.Dims[0],
                .height = key.Dims[1],
                .layerCount = 1,
                .viewFormatCount = static_cast<uint32_t>(colorFormats.size()),
                .pViewFormats = colorFormats.data()
            });
        }

        VkFramebufferAttachmentsCreateInfo attachmentsCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
            .pNext = nullptr,
            .attachmentImageInfoCount = static_cast<uint32_t>(attachmentImageInfos.size()),
            .pAttachmentImageInfos = attachmentImageInfos.data()
        };

        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = &attachmentsCreateInfo,
            .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
            .renderPass = RenderPass,
            .attachmentCount = static_cast<uint32_t>(attachmentImageInfos.size()),
            .pAttachments = nullptr,
            .width = key.Dims[0],
            .height = key.Dims[1],
            .layers = 1
        };

        VkFramebuffer framebuffer;
        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        Entries[key] = framebuffer;
        return framebuffer;
    }

    size_t GetColorAttachmentCount() const { return ColorAttachmentCount; }
};

struct RenderPassDesc {
    const std::vector<RenderPassAttachmentDesc>* ColorAttachments;
    std::optional<RenderPassAttachmentDesc> DepthAttachment;
};

class RenderPass {
public:
    VkRenderPass Raw;
    FramebufferCache FbCache;

    RenderPass(VkRenderPass renderPass, const FramebufferCache& framebufferCache)
        : Raw(renderPass)
        , FbCache(framebufferCache) {}
};

template<typename ShaderCode>
class PipelineShader {
public:
    ShaderCode Code;
    PipelineShaderDesc Desc;

    PipelineShader(const ShaderCode& code, const PipelineShaderDesc::Builder& descBuilder)
        : Code(code)
        , Desc(descBuilder.Build()) {}

    PipelineShader Clone() const {
        return PipelineShader(Code, PipelineShaderDesc::CreateBuilder(Desc.Stage)
            .SetDescriptorSetLayoutFlags(Desc.DescriptorSetLayoutFlags)
            .SetPushConstantsBytes(Desc.PushConstantsBytes)
            .SetEntry(Desc.Entry)
            .SetSource(Desc.Source));
    }
};

std::pair<std::vector<VkDescriptorSetLayout>, std::vector<std::unordered_map<uint32_t, VkDescriptorType>>>
CreateDescriptorSetLayouts(
    Device* device,
    const StageDescriptorSetLayouts* descriptorSets,
    VkShaderStageFlags stageFlags,
    const std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS>& setOpts);

ComputePipeline CreateComputePipeline(
    Device* device,
    const std::vector<uint8_t>& spirv,
    const ComputePipelineDesc& desc);

std::shared_ptr<RenderPass> CreateRenderPass(
    Device* device,
    const RenderPassDesc& desc);

RasterPipeline CreateRasterPipeline(
    Device* device,
    const std::vector<PipelineShader<std::vector<uint8_t>>>& shaders,
    const RasterPipelineDesc& desc);

StageDescriptorSetLayouts MergeShaderStageLayouts(
    const std::vector<StageDescriptorSetLayouts>& stages);

} // namespace tekki::backend::vulkan

// Hash specialization for ShaderSource
namespace std {
    template<>
    struct hash<tekki::backend::vulkan::ShaderSource> {
        std::size_t operator()(const tekki::backend::vulkan::ShaderSource& source) const {
            std::size_t seed = std::hash<int>{}(static_cast<int>(source.GetType()));
            if (source.GetType() == tekki::backend::vulkan::ShaderSourceType::Rust) {
                seed ^= std::hash<std::string>{}(source.GetRustEntry()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            } else {
                seed ^= std::hash<std::string>{}(source.GetHlslPath().string()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}