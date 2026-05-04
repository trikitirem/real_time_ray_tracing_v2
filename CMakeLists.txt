cmake_minimum_required(VERSION 4.2)
project(real_time_ray_tracing_v2)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

find_package(Vulkan REQUIRED)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

add_executable(real_time_ray_tracing_v2
        main.cpp
        src/engine/window.cpp
        src/engine/engine.cpp
        src/renderer/shared/device_context.cpp
        src/renderer/ray_tracing/device_config.hpp
)

target_include_directories(real_time_ray_tracing_v2 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(real_time_ray_tracing_v2 PRIVATE
        glfw
        glm::glm
        Vulkan::Vulkan
)

target_compile_definitions(real_time_ray_tracing_v2 PRIVATE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_CONSTRUCTORS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)
