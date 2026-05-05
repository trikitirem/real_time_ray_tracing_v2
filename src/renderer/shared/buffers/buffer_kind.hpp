#pragma once

namespace renderer::buffers {

enum class BufferKind {
    vertex,
    index,
    rt_vertex_as_input,
    rt_index_as_input,
    uniform,
    storage,
};

} // namespace renderer::buffers
