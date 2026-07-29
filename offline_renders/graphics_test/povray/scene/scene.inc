#version 3.7;
global_settings { assumed_gamma 1.0 ambient_light rgb <0.12, 0.12, 0.12> }
background { color rgb <0.53, 0.81, 0.98> }

// Directional sun light
light_source {
  <-0.000000, 707.106781, 707.106781>
  color rgb <1, 1, 1>
  parallel
  point_at <0, 0, 0>
}

// Plane primitive #0
object {
  mesh2 {
  vertex_vectors {
    4,
    <10.000000, 0.000000, -10.000000>,
    <-10.000000, 0.000000, -10.000000>,
    <-10.000000, 0.000000, 10.000000>,
    <10.000000, 0.000000, 10.000000>
  }
  normal_vectors {
    4,
    <-0.000000, 1.000000, 0.000000>,
    <-0.000000, 1.000000, 0.000000>,
    <-0.000000, 1.000000, 0.000000>,
    <-0.000000, 1.000000, 0.000000>
  }
  uv_vectors {
    4,
    <0.000000, 1.000000>,
    <1.000000, 1.000000>,
    <1.000000, 0.000000>,
    <0.000000, 0.000000>
  }
  face_indices {
    2,
    <0, 1, 2>,
    <0, 2, 3>
  }
  uv_indices {
    2,
    <0, 1, 2>,
    <0, 2, 3>
  }
  }
  texture {
    pigment { uv_mapping image_map { jpeg "assets/interior_tiles_diff_4k.jpg" gamma srgb } }
    finish { diffuse 1.000000 ambient 1.000000 specular 0.070000 roughness 0.090000 }
  }
}

// Plane primitive #1
object {
  mesh2 {
  vertex_vectors {
    4,
    <0.000048, 0.000000, -9.999952>,
    <9.999952, 0.000000, -0.000048>,
    <9.999952, 4.800000, -0.000048>,
    <0.000048, 4.800000, -9.999952>
  }
  normal_vectors {
    4,
    <-0.707107, -0.000000, 0.707107>,
    <-0.707107, -0.000000, 0.707107>,
    <-0.707107, -0.000000, 0.707107>,
    <-0.707107, -0.000000, 0.707107>
  }
  uv_vectors {
    4,
    <0.000000, 1.000000>,
    <1.000000, 1.000000>,
    <1.000000, 0.000000>,
    <0.000000, 0.000000>
  }
  face_indices {
    2,
    <0, 1, 2>,
    <0, 2, 3>
  }
  uv_indices {
    2,
    <0, 1, 2>,
    <0, 2, 3>
  }
  }
  texture {
    pigment { color rgb <0.920000, 0.920000, 0.950000> }
    finish { diffuse 0.000000 ambient 0.020000 specular 0.013720 roughness 0.002000 reflection { 0.980000 } }
  }
}

#include "duck_mesh2.inc"
object {
  Duck_Mesh
  texture {
    pigment { uv_mapping image_map { png "assets/DuckCM.png" gamma srgb } }
    finish { diffuse 1.000000 ambient 1.000000 specular 0.420000 roughness 0.040000 }
  }
}
