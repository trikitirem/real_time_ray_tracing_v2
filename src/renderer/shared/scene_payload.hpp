#pragma once

#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace renderer {

enum class BackendKind {
    raster,
    ray_tracing,
};

struct ScenePayload {
    BackendKind backend = BackendKind::raster;
    std::unique_ptr<void, void (*)(void*)> storage{nullptr, +[](void*) {}};
    const std::type_info* type = &typeid(void);

    template <typename T>
    void set(T&& value)
    {
        using U = std::decay_t<T>;
        U* ptr = new U(std::forward<T>(value));
        storage = std::unique_ptr<void, void (*)(void*)>(
            ptr,
            +[](void* p) { delete static_cast<U*>(p); });
        type = &typeid(U);
    }

    template <typename T>
    T* get_if()
    {
        using U = std::decay_t<T>;
        if (type != &typeid(U)) {
            return nullptr;
        }
        return static_cast<U*>(storage.get());
    }

    template <typename T>
    const T* get_if() const
    {
        using U = std::decay_t<T>;
        if (type != &typeid(U)) {
            return nullptr;
        }
        return static_cast<const U*>(storage.get());
    }
};

} // namespace renderer
