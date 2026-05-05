#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan_raii.hpp>

namespace renderer::descriptors {

struct UniformSetConfig {
    std::span<const vk::DescriptorSetLayoutBinding> bindings{};
    std::span<const vk::DescriptorPoolSize>         pool_sizes{};
    std::uint32_t                                   set_index = 0;
    std::uint32_t                                   max_sets  = 1;
};

class UniformSet {
public:
    UniformSet(const vk::raii::Device& device, const UniformSetConfig& cfg);
    UniformSet(const UniformSet&)            = delete;
    UniformSet& operator=(const UniformSet&) = delete;
    UniformSet(UniformSet&&)                 = default;
    UniformSet& operator=(UniformSet&&)      = default;
    ~UniformSet()                            = default;

    [[nodiscard]] const vk::raii::DescriptorSetLayout& layout() const { return layout_; }
    [[nodiscard]] vk::DescriptorSet                    set() const
    {
        if (sets_.empty()) {
            return vk::DescriptorSet{};
        }
        return *sets_.front();
    }
    [[nodiscard]] std::uint32_t                        set_index() const { return set_index_; }

    void update_uniform_buffer(std::uint32_t binding,
                               vk::Buffer    buffer,
                               vk::DeviceSize range,
                               vk::DeviceSize offset = 0) const;

    void update_storage_buffer(std::uint32_t binding,
                               vk::Buffer    buffer,
                               vk::DeviceSize range,
                               vk::DeviceSize offset = 0) const;

    void update_combined_image_sampler(std::uint32_t binding,
                                       vk::ImageView image_view,
                                       vk::Sampler   sampler,
                                       vk::ImageLayout image_layout
                                       = vk::ImageLayout::eShaderReadOnlyOptimal) const;
    void update_sampled_image(std::uint32_t binding,
                              vk::ImageView image_view,
                              vk::ImageLayout image_layout
                              = vk::ImageLayout::eShaderReadOnlyOptimal) const;
    void update_sampled_images(std::uint32_t binding,
                               std::span<const vk::DescriptorImageInfo> image_infos) const;
    void update_sampler(std::uint32_t binding, vk::Sampler sampler) const;

    void bind(vk::CommandBuffer  cmd,
              vk::PipelineLayout pipeline_layout,
              vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eGraphics) const;

private:
    const vk::raii::Device* device_ = nullptr;
    std::uint32_t           set_index_ = 0;

    vk::raii::DescriptorPool      pool_   = nullptr;
    vk::raii::DescriptorSetLayout layout_ = nullptr;
    vk::raii::DescriptorSets      sets_   = nullptr;
};

} // namespace renderer::descriptors
