struct CameraUbo {
    float4x4 viewProj;
    float3 viewPosition;
    uint _pad0;
};

struct ModelPushConstant {
    float4x4 model;
    float4 albedo;
    uint materialIndex;
    uint hasTexture;
    uint _pad0;
    uint _pad1;
};

struct MaterialGpu {
    float4 albedo;
    uint textureIndex;
    uint hasTexture;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

struct ReflectionInstanceLutGpu {
    uint materialIndex;
    uint indexOffset;
};

struct VsInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraUbo> gCamera;

[[vk::binding(1, 0)]]
RaytracingAccelerationStructure gAccelerationStructure;

[[vk::binding(2, 0)]]
StructuredBuffer<MaterialGpu> gMaterials;

[[vk::binding(3, 0)]]
StructuredBuffer<uint> gReflectionIndices;

[[vk::binding(4, 0)]]
StructuredBuffer<float2> gReflectionUvs;

[[vk::binding(5, 0)]]
StructuredBuffer<ReflectionInstanceLutGpu> gReflectionInstanceLut;

[[vk::push_constant]]
ModelPushConstant gModel;

[[vk::binding(0, 1)]]
Texture2D<float4> gTextures[];

[[vk::binding(1, 1)]]
SamplerState gAlbedoSampler;

static const float kEpsilon = 0.01;
static const float3 kSkyColor = float3(0.53, 0.81, 0.98);

float2 intersectionUv(uint instanceID, uint primitiveIndex, float2 barycentrics) {
    uint lutCount = 0;
    uint lutStride = 0;
    gReflectionInstanceLut.GetDimensions(lutCount, lutStride);
    if (instanceID >= lutCount) {
        return float2(0.0, 0.0);
    }

    ReflectionInstanceLutGpu lutEntry = gReflectionInstanceLut[instanceID];
    uint baseIndex = lutEntry.indexOffset + primitiveIndex * 3;

    uint indexCount = 0;
    uint indexStride = 0;
    gReflectionIndices.GetDimensions(indexCount, indexStride);
    if (baseIndex + 2 >= indexCount) {
        return float2(0.0, 0.0);
    }

    uint i0 = gReflectionIndices[baseIndex + 0];
    uint i1 = gReflectionIndices[baseIndex + 1];
    uint i2 = gReflectionIndices[baseIndex + 2];

    uint uvCount = 0;
    uint uvStride = 0;
    gReflectionUvs.GetDimensions(uvCount, uvStride);
    if (i0 >= uvCount || i1 >= uvCount || i2 >= uvCount) {
        return float2(0.0, 0.0);
    }

    float2 uv0 = gReflectionUvs[i0];
    float2 uv1 = gReflectionUvs[i1];
    float2 uv2 = gReflectionUvs[i2];
    float w0 = 1.0 - barycentrics.x - barycentrics.y;
    float w1 = barycentrics.x;
    float w2 = barycentrics.y;
    return w0 * uv0 + w1 * uv1 + w2 * uv2;
}

float3 sampleReflectionColor(float3 worldPos, float3 N, float3 V) {
    RayDesc reflectionRayDesc;
    reflectionRayDesc.Origin = worldPos + N * kEpsilon;
    reflectionRayDesc.Direction = reflect(-V, N);
    reflectionRayDesc.TMin = kEpsilon;
    reflectionRayDesc.TMax = 1e4;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rq;
    let reflectionFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;
    rq.TraceRayInline(gAccelerationStructure, reflectionFlags, 0xFF, reflectionRayDesc);
    rq.Proceed();

    if (rq.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
        return kSkyColor;
    }

    uint instanceID = rq.CommittedRayInstanceCustomIndex();
    uint lutCount = 0;
    uint lutStride = 0;
    gReflectionInstanceLut.GetDimensions(lutCount, lutStride);
    if (instanceID >= lutCount) {
        return kSkyColor;
    }
    ReflectionInstanceLutGpu lutEntry = gReflectionInstanceLut[instanceID];
    uint materialIndex = lutEntry.materialIndex;
    uint materialCount = 0;
    uint materialStride = 0;
    gMaterials.GetDimensions(materialCount, materialStride);
    if (materialIndex >= materialCount) {
        return kSkyColor;
    }

    MaterialGpu hitMaterial = gMaterials[materialIndex];
    if (hitMaterial.hasTexture != 0) {
        uint primitiveIndex = rq.CommittedPrimitiveIndex();
        float2 bary = rq.CommittedTriangleBarycentrics();
        float2 hitUv = intersectionUv(instanceID, primitiveIndex, bary);
        return gTextures[NonUniformResourceIndex(hitMaterial.textureIndex)].SampleLevel(gAlbedoSampler, hitUv, 0).rgb;
    }
    return hitMaterial.albedo.rgb;
}

[shader("vertex")]
VertexOutput vertMain(VsInput input) {
    VertexOutput o;
    float4 worldPos = mul(gModel.model, float4(input.position, 1.0));
    o.position = mul(gCamera.viewProj, worldPos);
    o.uv = input.uv;
    o.normal = input.normal;
    o.worldPos = worldPos.xyz;
    return o;
}

[shader("fragment")]
float4 fragMain(VertexOutput v) : SV_Target {
    const float3 lightDirToLight = normalize(float3(0.0, 1.0, 1.0));
    const float3 lightColor = float3(1.0, 1.0, 1.0);
    const float ambientStrength = 0.12;
    const float specularStrength = 0.7;
    const float shininess = 24.0;

    float3 baseColor = gModel.albedo.rgb;
    MaterialGpu drawMaterial;
    drawMaterial.albedo = float4(1.0, 1.0, 1.0, 1.0);
    drawMaterial.textureIndex = 0;
    drawMaterial.hasTexture = 0;
    drawMaterial._pad0 = 0;
    drawMaterial._pad1 = 0;
    uint materialCount = 0;
    uint materialStride = 0;
    gMaterials.GetDimensions(materialCount, materialStride);
    if (gModel.materialIndex < materialCount) {
        drawMaterial = gMaterials[gModel.materialIndex];
    }
    if (drawMaterial.hasTexture != 0) {
        float4 tex = gTextures[NonUniformResourceIndex(drawMaterial.textureIndex)].Sample(gAlbedoSampler, v.uv);
        baseColor = tex.rgb;
    }

    float3 N = normalize(v.normal);
    float3 L = lightDirToLight;
    float3 V = normalize(gCamera.viewPosition - v.worldPos);
    float3 H = normalize(L + V);
    const float shadowStrength = 0.2;

    float3 ambient = ambientStrength * lightColor * baseColor;
    float ndotl = saturate(dot(N, L));
    float3 diffuse = ndotl * lightColor * baseColor;
    float spec = pow(saturate(dot(N, H)), shininess);
    float3 specular = specularStrength * spec * lightColor;

    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = v.worldPos;
    shadowRayDesc.Direction = normalize(lightDirToLight);
    shadowRayDesc.TMin = 0.01;
    shadowRayDesc.TMax = 1e4;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> sq;
    let shadowRayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    sq.TraceRayInline(gAccelerationStructure, shadowRayFlags, 0xFF, shadowRayDesc);
    sq.Proceed();

    bool inShadow = (sq.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
    if (inShadow) {
        diffuse *= shadowStrength;
        specular *= shadowStrength;
    }

    float3 litColor = ambient + diffuse + specular;
    if (gModel.hasTexture == 0) {
        float3 reflected = sampleReflectionColor(v.worldPos, N, V);
        litColor = lerp(litColor, reflected, 0.65);
    }
    return float4(litColor, 1.0f);
}
