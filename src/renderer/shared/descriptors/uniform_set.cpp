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
    pool_ci.maxSets       = cfg.max_sets;
    pool_ci.poolSizeCount = static_cast<std::uint32_t>(cfg.pool_sizes.size());
    pool_ci.pPoolSizes    = cfg.pool_sizes.data();
    pool_                 = vk::raii::DescriptorPool(device, pool_ci);

    const std::array<vk::DescriptorSetLayout, 1> layouts = { *layout_ };
    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool     = *pool_;
    alloc_info.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    alloc_info.pSetLayouts        = layouts.data();
    set_ = device.allocateDescriptorSets(alloc_info).front();
}

void UniformSet::update_uniform_buffer(const std::uint32_t binding,
                                       const vk::Buffer    buffer,
                                       const vk::DeviceSize range,
                                       const vk::DeviceSize offset) const
{
    update_buffer_descriptor(*device_, set_, binding, vk::DescriptorType::eUniformBuffer, buffer,
                             range, offset);
}

void UniformSet::update_storage_buffer(const std::uint32_t binding,
                                       const vk::Buffer    buffer,
                                       const vk::DeviceSize range,
                                       const vk::DeviceSize offset) const
{
    update_buffer_descriptor(*device_, set_, binding, vk::DescriptorType::eStorageBuffer, buffer,
                             range, offset);
}

void UniformSet::bind(const vk::CommandBuffer   cmd,
                      const vk::PipelineLayout  pipeline_layout,
                      const vk::PipelineBindPoint bind_point) const
{
    const std::array<vk::DescriptorSet, 1> sets = { set_ };
    cmd.bindDescriptorSets(bind_point, pipeline_layout, set_index_, sets, {});
}

} // namespace renderer::descriptors
