#pragma once

#include "scene/instanced_group.hpp"
#include "scene/model.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace scene {

class Scene {
public:
    Scene() = default;

    void add_model(Model model) { models_.push_back(std::move(model)); }

    [[nodiscard]] std::size_t model_count() const { return models_.size(); }

    [[nodiscard]] Model& get_model(std::size_t index) { return models_.at(index); }
    [[nodiscard]] const Model& get_model(std::size_t index) const
    {
        return models_.at(index);
    }

    [[nodiscard]] std::vector<Model>& models() { return models_; }
    [[nodiscard]] const std::vector<Model>& models() const { return models_; }

    void add_instanced_group(InstancedGroup group)
    {
        instanced_groups_.push_back(std::move(group));
    }

    [[nodiscard]] const std::vector<InstancedGroup>& instanced_groups() const
    {
        return instanced_groups_;
    }

private:
    std::vector<Model> models_{};
    std::vector<InstancedGroup> instanced_groups_{};
};

} // namespace scene
