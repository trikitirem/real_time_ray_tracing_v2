#pragma once

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

private:
    std::vector<Model> models_{};
};

} // namespace scene
