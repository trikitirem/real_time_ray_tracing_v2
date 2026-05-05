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
        add_custom_command(
                OUTPUT ${spv}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
                COMMAND ${SLANGC}
                    ${slang_file}
                    -target spirv
                    -profile spirv_1_4
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    -entry vertMain
                    -entry fragMain
                    -o ${spv}
                DEPENDS ${slang_file}
                COMMENT "Slang ${name_we}.slang -> ${spv}"
                VERBATIM
        )
        list(APPEND RTRT_SHADER_SPV ${spv})
    endforeach()
endforeach()
add_custom_target(rtrt_shaders ALL DEPENDS ${RTRT_SHADER_SPV})

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
        src/renderer/shared/vulkan_dynamic_dispatch.cpp
        src/renderer/shared/swapchain.cpp
        src/renderer/shared/shader_module_util.cpp
        src/renderer/shared/graphics_pipeline.cpp
        src/renderer/shared/renderer.cpp
        src/renderer/raster/raster_pipeline.cpp
        src/renderer/raster/raster_frame_recorder.cpp
        src/renderer/ray_tracing/rt_pipeline.cpp
        src/renderer/ray_tracing/rt_frame_recorder.cpp
        src/renderer/ray_tracing/device_config.hpp
        src/renderer/shared/viewport_flip.hpp
)

add_dependencies(real_time_ray_tracing_v2 rtrt_shaders)

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
        "SHADER_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\""
)
