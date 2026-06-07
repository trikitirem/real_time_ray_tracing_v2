$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$positions = @( -0.5, 0.0, -0.5, 0.5, 0.0, -0.5, 0.5, 0.0, 0.5, -0.5, 0.0, 0.5 )
$normals = @(0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0)
$uvs = @(0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0)
$indices = @(0, 1, 2, 0, 2, 3)
$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
foreach ($v in $positions) { $bw.Write([single]$v) }
foreach ($v in $normals) { $bw.Write([single]$v) }
foreach ($v in $uvs) { $bw.Write([single]$v) }
foreach ($i in $indices) { $bw.Write([uint16]$i) }
[System.IO.File]::WriteAllBytes((Join-Path $root 'textured_quad.bin'), $ms.ToArray())
$gltf = @'
{
  "asset": { "version": "2.0", "generator": "rtrt" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{ "primitives": [{ "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 3, "material": 0 }] }],
  "materials": [{ "pbrMetallicRoughness": { "baseColorTexture": { "index": 0 }, "metallicFactor": 0.0, "roughnessFactor": 0.8 } }],
  "textures": [{ "source": 0 }],
  "images": [{ "uri": "../images/texture.jpg" }],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "max": [0.5, 0.0, 0.5], "min": [-0.5, 0.0, -0.5] },
    { "bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2" },
    { "bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR" }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 48, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 96, "byteLength": 32 },
    { "buffer": 0, "byteOffset": 128, "byteLength": 12 }
  ],
  "buffers": [{ "byteLength": 140, "uri": "textured_quad.bin" }]
}
'@
Set-Content -Path (Join-Path $root 'textured_quad.gltf') -Value $gltf -Encoding UTF8
Write-Output "Generated textured_quad in $root"
