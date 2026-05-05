#include "renderer/shared/textures/texture_resource.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include <stb_image.h>

namespace renderer::textures {

std::uint32_t TextureResource::find_memory_type(
    const vk::PhysicalDeviceMemoryProperties& mem_props,
    const std::uint32_t                       type_bits,
    const vk::MemoryPropertyFlags             required_properties)
{
    for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        const bool supported = (type_bits & (1u << i)) != 0;
        const bool has_props = (mem_props.memoryTypes[i].propertyFlags & required_properties)
                            == required_properties;
        if (supported && has_props) {
            return i;
        }
    }
    throw std::runtime_error("TextureResource::find_memory_type: no suitable memory type");
}

void TextureResource::transition_image_layout_one_shot(const vk::raii::Device&      device,
                                                       const vk::raii::CommandPool& command_pool,
                                                       const vk::raii::Queue&       queue,
                                                       const vk::Image              image,
                                                       const vk::ImageLayout        old_layout,
                                                       const vk::ImageLayout        new_layout)
{
    vk::PipelineStageFlags2 src_stage{};
    vk::PipelineStageFlags2 dst_stage{};
    vk::AccessFlags2        src_access{};
    vk::AccessFlags2        dst_access{};

    if (old_layout == vk::ImageLayout::eUndefined
        && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        src_stage  = vk::PipelineStageFlagBits2::eTopOfPipe;
        dst_stage  = vk::PipelineStageFlagBits2::eTransfer;
        src_access = {};
        dst_access = vk::AccessFlagBits2::eTransferWrite;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal
               && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        src_stage  = vk::PipelineStageFlagBits2::eTransfer;
        dst_stage  = vk::PipelineStageFlagBits2::eFragmentShader;
        src_access = vk::AccessFlagBits2::eTransferWrite;
        dst_access = vk::AccessFlagBits2::eShaderRead;
    } else {
        throw std::runtime_error("TextureResource: unsupported image layout transition");
    }

    vk::CommandBufferAllocateInfo alloc{};
    alloc.commandPool        = *command_pool;
    alloc.level              = vk::CommandBufferLevel::ePrimary;
    alloc.commandBufferCount = 1;
    vk::raii::CommandBuffers cmd_buffers(device, alloc);
    vk::raii::CommandBuffer& cmd = cmd_buffers.front();

    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    const vk::ImageMemoryBarrier2 barrier{
        .srcStageMask        = src_stage,
        .srcAccessMask       = src_access,
        .dstStageMask        = dst_stage,
        .dstAccessMask       = dst_access,
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = vk::ImageSubresourceRange{
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    });
    cmd.end();

    const vk::CommandBuffer cmd_raw = *cmd;
    queue.submit(vk::SubmitInfo{
                     .commandBufferCount = 1,
                     .pCommandBuffers    = &cmd_raw,
                 },
                 nullptr);
    queue.waitIdle();
}

void TextureResource::copy_buffer_to_image_one_shot(const vk::raii::Device&      device,
                                                    const vk::raii::CommandPool& command_pool,
                                                    const vk::raii::Queue&       queue,
                                                    const vk::Buffer             src_buffer,
                                                    const vk::Image              dst_image,
                                                    const vk::Extent2D           extent)
{
    vk::CommandBufferAllocateInfo alloc{};
    alloc.commandPool        = *command_pool;
    alloc.level              = vk::CommandBufferLevel::ePrimary;
    alloc.commandBufferCount = 1;
    vk::raii::CommandBuffers cmd_buffers(device, alloc);
    vk::raii::CommandBuffer& cmd = cmd_buffers.front();

    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    const vk::BufferImageCopy region{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = vk::ImageSubresourceLayers{
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
        .imageOffset = vk::Offset3D{ 0, 0, 0 },
        .imageExtent = vk::Extent3D{ extent.width, extent.height, 1 },
    };
    cmd.copyBufferToImage(src_buffer, dst_image, vk::ImageLayout::eTransferDstOptimal, region);
    cmd.end();

    const vk::CommandBuffer cmd_raw = *cmd;
    queue.submit(vk::SubmitInfo{
                     .commandBufferCount = 1,
                     .pCommandBuffers    = &cmd_raw,
                 },
                 nullptr);
    queue.waitIdle();
}

TextureResource TextureResource::load_from_asset_location(
    const vk::raii::PhysicalDevice& physical_device,
    const vk::raii::Device&         device,
    const vk::raii::CommandPool&    command_pool,
    const vk::raii::Queue&          graphics_queue,
    const util::AssetLocation&      location)
{
    TextureResource out{};

    const std::filesystem::path full_path = location.full_path();
    const std::string           path_str  = full_path.string();

    int      width = 0;
    int      height = 0;
    int      channels = 0;
    stbi_uc* pixels = stbi_load(path_str.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        throw std::runtime_error("TextureResource: failed to load image: " + path_str);
    }

    const vk::DeviceSize image_size
        = static_cast<vk::DeviceSize>(width) * static_cast<vk::DeviceSize>(height) * 4;

    vk::BufferCreateInfo staging_ci{};
    staging_ci.size        = image_size;
    staging_ci.usage       = vk::BufferUsageFlagBits::eTransferSrc;
    staging_ci.sharingMode = vk::SharingMode::eExclusive;
    vk::raii::Buffer staging_buffer(device, staging_ci);

    const vk::MemoryRequirements staging_req = staging_buffer.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties mem_props = physical_device.getMemoryProperties();
    const std::uint32_t staging_mem_type = find_memory_type(
        mem_props, staging_req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryAllocateInfo staging_alloc{};
    staging_alloc.allocationSize  = staging_req.size;
    staging_alloc.memoryTypeIndex = staging_mem_type;
    vk::raii::DeviceMemory staging_memory(device, staging_alloc);
    staging_buffer.bindMemory(*staging_memory, 0);

    void* mapped = staging_memory.mapMemory(0, image_size);
    std::memcpy(mapped, pixels, static_cast<std::size_t>(image_size));
    staging_memory.unmapMemory();
    stbi_image_free(pixels);

    out.extent_ = vk::Extent2D{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };
    out.format_ = vk::Format::eR8G8B8A8Srgb;

    vk::ImageCreateInfo image_ci{};
    image_ci.imageType     = vk::ImageType::e2D;
    image_ci.extent        = vk::Extent3D{ out.extent_.width, out.extent_.height, 1 };
    image_ci.mipLevels     = 1;
    image_ci.arrayLayers   = 1;
    image_ci.format        = out.format_;
    image_ci.tiling        = vk::ImageTiling::eOptimal;
    image_ci.initialLayout = vk::ImageLayout::eUndefined;
    image_ci.usage         = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    image_ci.samples       = vk::SampleCountFlagBits::e1;
    image_ci.sharingMode   = vk::SharingMode::eExclusive;
    out.image_             = vk::raii::Image(device, image_ci);

    const vk::MemoryRequirements image_req = out.image_.getMemoryRequirements();
    const std::uint32_t image_mem_type = find_memory_type(mem_props, image_req.memoryTypeBits,
                                                          vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::MemoryAllocateInfo image_alloc{};
    image_alloc.allocationSize  = image_req.size;
    image_alloc.memoryTypeIndex = image_mem_type;
    out.memory_                 = vk::raii::DeviceMemory(device, image_alloc);
    out.image_.bindMemory(*out.memory_, 0);

    transition_image_layout_one_shot(device, command_pool, graphics_queue, *out.image_,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal);
    copy_buffer_to_image_one_shot(device, command_pool, graphics_queue, *staging_buffer,
                                  *out.image_, out.extent_);
    transition_image_layout_one_shot(device, command_pool, graphics_queue, *out.image_,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal);

    const vk::ImageViewCreateInfo view_ci{
        .image            = *out.image_,
        .viewType         = vk::ImageViewType::e2D,
        .format           = out.format_,
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    out.view_ = vk::raii::ImageView(device, view_ci);

    const vk::PhysicalDeviceProperties properties = physical_device.getProperties();
    const vk::SamplerCreateInfo sampler_ci{
        .magFilter               = vk::Filter::eLinear,
        .minFilter               = vk::Filter::eLinear,
        .mipmapMode              = vk::SamplerMipmapMode::eLinear,
        .addressModeU            = vk::SamplerAddressMode::eRepeat,
        .addressModeV            = vk::SamplerAddressMode::eRepeat,
        .addressModeW            = vk::SamplerAddressMode::eRepeat,
        .anisotropyEnable        = vk::True,
        .maxAnisotropy           = properties.limits.maxSamplerAnisotropy,
        .borderColor             = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False,
        .compareEnable           = vk::False,
        .compareOp               = vk::CompareOp::eAlways,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .mipLodBias              = 0.0f,
    };
    out.sampler_ = vk::raii::Sampler(device, sampler_ci);

    return out;
}

} // namespace renderer::textures
