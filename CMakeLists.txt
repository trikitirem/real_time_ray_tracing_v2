cmake_minimum_required(VERSION 4.2)
project(real_time_ray_tracing_v2)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

find_package(Vulkan REQUIRED)

find_program(SLANGC
        NAMES slangc slangc.exe
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        REQUIRED
)

# Every *.slang → one .spv (vertMain + fragMain), matching Zig build.zig slangc invocation.
set(RTRT_SHADER_SPV "")
foreach(rel IN ITEMS shaders/raster shaders/ray_tracing)
    set(shader_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/${rel}")
    file(GLOB slang_files CONFIGURE_DEPENDS "${shader_src_dir}/*.slang")
    foreach(slang_file IN LISTS slang_files)
        get_filename_component(name_we ${slang_file} NAME_WE)
        set(out_dir "${CMAKE_BINARY_DIR}/${rel}")
        set(spv "${out_dir}/${name_we}.spv")
        set(shader_entries -entry vertMain -entry fragMain)
        if(name_we STREQUAL "default" AND rel STREQUAL "shaders/raster")
            list(APPEND shader_entries -entry vertMainInstanced -entry fragMainInstanced)
        elseif(name_we STREQUAL "shadow" AND rel STREQUAL "shaders/raster")
            list(APPEND shader_entries -entry vertMainInstanced)
        endif()
        add_custom_command(
                OUTPUT ${spv}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
                COMMAND ${SLANGC}
                    ${slang_file}
                    -target spirv
                    -profile spirv_1_4
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    ${shader_entries}
                    -o ${spv}
                DEPENDS ${slang_file}
                COMMENT "Slang ${name_we}.slang -> ${spv}"
                VERBATIM
        )
        list(APPEND RTRT_SHADER_SPV ${spv})
    endforeach()
endforeach()
add_custom_target(rtrt_shaders ALL DEPENDS ${RTRT_SHADER_SPV})

# Runtime assets: copy images/ next to built binaries.
add_custom_target(rtrt_images ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/images"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "${CMAKE_BINARY_DIR}/images"
        COMMENT "Copy images -> build/images"
)

add_custom_target(rtrt_scenes ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/scenes"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scenes"
            "${CMAKE_BINARY_DIR}/scenes"
        COMMENT "Copy scenes -> build/scenes"
)

add_custom_target(rtrt_models ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/models"
            "${CMAKE_BINARY_DIR}/models"
        COMMENT "Copy models -> build/models"
)

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

# nlohmann/json
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# tinygltf (header-only)
FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG        v2.9.3
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinygltf)

# STBImage

add_executable(real_time_ray_tracing_v2
        main.cpp
        src/engine/window.cpp
        src/engine/input_controller.cpp
        src/engine/engine.cpp
        src/engine/benchmark.cpp
        src/engine/camera_animator.cpp
        src/util/asset_root.cpp
        src/scene/scene_registry.cpp
        src/scene/scene_loader.cpp
        src/scene/gltf_loader.cpp
        src/renderer/shared/device_context.cpp
        src/renderer/shared/vulkan_dynamic_dispatch.cpp
        src/renderer/shared/swapchain.cpp
        src/renderer/shared/shader_module_util.cpp
        src/renderer/shared/graphics_pipeline.cpp
        src/renderer/shared/renderer.cpp
        src/renderer/shared/descriptors/uniform_set.cpp
        src/renderer/shared/textures/texture_resource.cpp
        src/renderer/shared/buffers/base_buffer.cpp
        src/renderer/shared/buffers/gpu_buffer.cpp
        src/renderer/shared/buffers/host_visible_buffer.cpp
        src/renderer/raster/raster_pipeline.cpp
        src/renderer/raster/raster_frame_recorder.cpp
        src/renderer/raster/raster_render_backend.cpp
        src/renderer/raster/raster_scene_gpu_builder.cpp
        src/renderer/ray_tracing/rt_pipeline.cpp
        src/renderer/ray_tracing/rt_frame_recorder.cpp
        src/renderer/ray_tracing/rt_render_backend.cpp
        src/renderer/ray_tracing/rt_scene_gpu_builder.cpp
        src/renderer/ray_tracing/blas_builder.cpp
        src/renderer/ray_tracing/tlas_builder.cpp
        src/renderer/ray_tracing/device_config.hpp
        src/renderer/shared/viewport_flip.hpp
        src/scene/mesh_primitive.hpp
        src/scene/model.hpp
        src/renderer/shared/descriptors/uniform_set.hpp
        src/renderer/ray_tracing/shader_config.hpp
)

add_dependencies(real_time_ray_tracing_v2 rtrt_shaders)
add_dependencies(real_time_ray_tracing_v2 rtrt_images)
add_dependencies(real_time_ray_tracing_v2 rtrt_scenes)
add_dependencies(real_time_ray_tracing_v2 rtrt_models)

target_include_directories(real_time_ray_tracing_v2 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
        ${tinygltf_SOURCE_DIR}
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(real_time_ray_tracing_v2 PRIVATE
        glfw
        glm::glm
        nlohmann_json::nlohmann_json
        Vulkan::Vulkan
)

add_custom_command(TARGET real_time_ray_tracing_v2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/measurements"
        COMMENT "Create measurements directory"
)

target_compile_definitions(real_time_ray_tracing_v2 PRIVATE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_CONSTRUCTORS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        "SHADER_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\""
)
