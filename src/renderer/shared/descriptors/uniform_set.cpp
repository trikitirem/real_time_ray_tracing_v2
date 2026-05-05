#include "renderer/shared/descriptors/uniform_set.hpp"

#include <array>
#include <stdexcept>

namespace renderer::descriptors {

namespace {

void update_buffer_descriptor(const vk::raii::Device& device,
                              const vk::DescriptorSet set,
                              const std::uint32_t     binding,
                              const vk::DescriptorType type,
                              const vk::Buffer        buffer,
                              const vk::DeviceSize    range,
                              const vk::DeviceSize    offset)
{
    vk::DescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range  = range;

    vk::WriteDescriptorSet write{};
    write.dstSet          = set;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pBufferInfo     = &buffer_info;

    const std::array<vk::WriteDescriptorSet, 1> writes = { write };
    device.updateDescriptorSets(writes, {});
}

void update_combined_image_sampler_descriptor(const vk::raii::Device& device,
                                              const vk::DescriptorSet set,
                                              const std::uint32_t     binding,
                                              const vk::ImageView     image_view,
                                              const vk::Sampler       sampler,
                                              const vk::ImageLayout   image_layout)
{
    const vk::DescriptorImageInfo image_info{
        .sampler     = sampler,
        .imageView   = image_view,
        .imageLayout = image_layout,
    };

    vk::WriteDescriptorSet write{};
    write.dstSet          = set;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo      = &image_info;

    const std::array<vk::WriteDescriptorSet, 1> writes = { write };
    device.updateDescriptorSets(writes, {});
}

void update_sampled_image_descriptor(const vk::raii::Device& device,
                                     const vk::DescriptorSet set,
                                     const std::uint32_t     binding,
                                     const vk::ImageView     image_view,
                                     const vk::ImageLayout   image_layout)
{
    const vk::DescriptorImageInfo image_info{
        .sampler     = vk::Sampler{},
        .imageView   = image_view,
        .imageLayout = image_layout,
    };

    vk::WriteDescriptorSet write{};
    write.dstSet          = set;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eSampledImage;
    write.pImageInfo      = &image_info;

    const std::array<vk::WriteDescriptorSet, 1> writes = { write };
    device.updateDescriptorSets(writes, {});
}

void update_sampler_descriptor(const vk::raii::Device& device,
                               const vk::DescriptorSet set,
                               const std::uint32_t     binding,
                               const vk::Sampler       sampler)
{
    const vk::DescriptorImageInfo image_info{
        .sampler     = sampler,
        .imageView   = vk::ImageView{},
        .imageLayout = vk::ImageLayout::eUndefined,
    };

    vk::WriteDescriptorSet write{};
    write.dstSet          = set;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eSampler;
    write.pImageInfo      = &image_info;

    const std::array<vk::WriteDescriptorSet, 1> writes = { write };
    device.updateDescriptorSets(writes, {});
}

} // namespace

UniformSet::UniformSet(const vk::raii::Device& device, const UniformSetConfig& cfg)
    : device_(&device)
    , set_index_(cfg.set_index)
{
    if (cfg.bindings.empty()) {
        throw std::runtime_error("UniformSet: at least one layout binding is required");
    }
    if (cfg.pool_sizes.empty()) {
        throw std::runtime_error("UniformSet: at least one descriptor pool size is required");
    }
    if (cfg.max_sets == 0) {
        throw std::runtime_error("UniformSet: max_sets must be greater than zero");
    }

    vk::DescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.bindingCount = static_cast<std::uint32_t>(cfg.bindings.size());
    layout_ci.pBindings    = cfg.bindings.data();
    layout_                = vk::raii::DescriptorSetLayout(device, layout_ci);

    vk::DescriptorPoolCreateInfo pool_ci{};
    pool_ci.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_ci.maxSets       = cfg.max_sets;
    pool_ci.poolSizeCount = static_cast<std::uint32_t>(cfg.pool_sizes.size());
    pool_ci.pPoolSizes    = cfg.pool_sizes.data();
    pool_                 = vk::raii::DescriptorPool(device, pool_ci);

    const std::array<vk::DescriptorSetLayout, 1> layouts = { *layout_ };
    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool     = *pool_;
    alloc_info.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    alloc_info.pSetLayouts        = layouts.data();
    sets_ = vk::raii::DescriptorSets(device, alloc_info);
    if (sets_.empty()) {
        throw std::runtime_error("UniformSet: descriptor set allocation returned no sets");
    }
}

void UniformSet::update_uniform_buffer(const std::uint32_t binding,
                                       const vk::Buffer    buffer,
                                       const vk::DeviceSize range,
                                       const vk::DeviceSize offset) const
{
    update_buffer_descriptor(*device_, *sets_.front(), binding, vk::DescriptorType::eUniformBuffer, buffer,
                             range, offset);
}

void UniformSet::update_storage_buffer(const std::uint32_t binding,
                                       const vk::Buffer    buffer,
                                       const vk::DeviceSize range,
                                       const vk::DeviceSize offset) const
{
    update_buffer_descriptor(*device_, *sets_.front(), binding, vk::DescriptorType::eStorageBuffer, buffer,
                             range, offset);
}

void UniformSet::update_combined_image_sampler(const std::uint32_t binding,
                                               const vk::ImageView image_view,
                                               const vk::Sampler   sampler,
                                               const vk::ImageLayout image_layout) const
{
    update_combined_image_sampler_descriptor(*device_, *sets_.front(), binding, image_view, sampler,
                                             image_layout);
}

void UniformSet::update_sampled_image(const std::uint32_t binding,
                                      const vk::ImageView image_view,
                                      const vk::ImageLayout image_layout) const
{
    update_sampled_image_descriptor(*device_, *sets_.front(), binding, image_view, image_layout);
}

void UniformSet::update_sampler(const std::uint32_t binding, const vk::Sampler sampler) const
{
    update_sampler_descriptor(*device_, *sets_.front(), binding, sampler);
}

void UniformSet::bind(const vk::CommandBuffer   cmd,
                      const vk::PipelineLayout  pipeline_layout,
                      const vk::PipelineBindPoint bind_point) const
{
    const std::array<vk::DescriptorSet, 1> sets = { *sets_.front() };
    cmd.bindDescriptorSets(bind_point, pipeline_layout, set_index_, sets, {});
}

} // namespace renderer::descriptors
