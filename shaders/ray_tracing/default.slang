struct CameraUbo {
    float4x4 viewProj;
    float3 viewPosition;
    uint reflections_enabled;
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
    float metalness;
    float roughness;
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

[[vk::binding(6, 0)]]
StructuredBuffer<float4> gReflectionNormals;

[[vk::push_constant]]
ModelPushConstant gModel;

[[vk::binding(0, 1)]]
Texture2D<float4> gTextures[];

[[vk::binding(1, 1)]]
SamplerState gAlbedoSampler;

static const float kEpsilon = 0.01;
static const float3 kSkyColor = float3(0.53, 0.81, 0.98);
static const float3 kLightDirToLight = normalize(float3(0.0, 1.0, 1.0));
static const float kShadowStrength = 0.2;
static const float3 kLightColor = float3(1.0, 1.0, 1.0);
static const float kAmbientStrength = 0.12;
static const float kSpecularStrength = 0.7;
static const float kShininess = 24.0;

bool traceShadow(float3 origin, float3 lightDir) {
    RayDesc shadowRayDesc;
    shadowRayDesc.Origin = origin;
    shadowRayDesc.Direction = lightDir;
    shadowRayDesc.TMin = kEpsilon;
    shadowRayDesc.TMax = 1e4;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> sq;
    let shadowRayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    sq.TraceRayInline(gAccelerationStructure, shadowRayFlags, 0xFF, shadowRayDesc);
    sq.Proceed();
    return sq.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

float3 shadePhong(float3 worldPos, float3 N, float3 baseColor, float3 V, float metalness, float roughness) {
    float3 L = kLightDirToLight;
    float3 H = normalize(L + V);

    float3 ambient = kAmbientStrength * kLightColor * baseColor;
    float3 diffuse = saturate(dot(N, L)) * kLightColor * baseColor;
    float specularWeight = 1.0 - roughness;
    float3 specular = kSpecularStrength * specularWeight
                      * pow(saturate(dot(N, H)), kShininess) * kLightColor;

    if (traceShadow(worldPos + N * kEpsilon, kLightDirToLight)) {
        diffuse *= kShadowStrength;
        specular *= kShadowStrength;
    }
    return ambient + diffuse + specular;
}

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

float3 intersectionNormal(uint instanceID, uint primitiveIndex, float2 barycentrics) {
    uint lutCount = 0;
    uint lutStride = 0;
    gReflectionInstanceLut.GetDimensions(lutCount, lutStride);
    if (instanceID >= lutCount) {
        return float3(0.0, 1.0, 0.0);
    }

    ReflectionInstanceLutGpu lutEntry = gReflectionInstanceLut[instanceID];
    uint baseIndex = lutEntry.indexOffset + primitiveIndex * 3;

    uint indexCount = 0;
    uint indexStride = 0;
    gReflectionIndices.GetDimensions(indexCount, indexStride);
    if (baseIndex + 2 >= indexCount) {
        return float3(0.0, 1.0, 0.0);
    }

    uint i0 = gReflectionIndices[baseIndex + 0];
    uint i1 = gReflectionIndices[baseIndex + 1];
    uint i2 = gReflectionIndices[baseIndex + 2];

    uint normalCount = 0;
    uint normalStride = 0;
    gReflectionNormals.GetDimensions(normalCount, normalStride);
    if (i0 >= normalCount || i1 >= normalCount || i2 >= normalCount) {
        return float3(0.0, 1.0, 0.0);
    }

    float3 n0 = gReflectionNormals[i0].xyz;
    float3 n1 = gReflectionNormals[i1].xyz;
    float3 n2 = gReflectionNormals[i2].xyz;
    float w0 = 1.0 - barycentrics.x - barycentrics.y;
    float w1 = barycentrics.x;
    float w2 = barycentrics.y;
    return normalize(w0 * n0 + w1 * n1 + w2 * n2);
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

    const uint primitiveIndex = rq.CommittedPrimitiveIndex();
    const float2 bary = rq.CommittedTriangleBarycentrics();
    const float3 hitPos = reflectionRayDesc.Origin + rq.CommittedRayT() * reflectionRayDesc.Direction;
    float3 hitN = intersectionNormal(instanceID, primitiveIndex, bary);
    if (dot(hitN, reflectionRayDesc.Direction) > 0.0) {
        hitN = -hitN;
    }

    MaterialGpu hitMaterial = gMaterials[materialIndex];
    float3 hitColor = hitMaterial.albedo.rgb;
    if (hitMaterial.hasTexture != 0) {
        float2 hitUv = intersectionUv(instanceID, primitiveIndex, bary);
        hitColor = gTextures[NonUniformResourceIndex(hitMaterial.textureIndex)].SampleLevel(gAlbedoSampler, hitUv, 0).rgb;
    }

    float3 V_hit = normalize(gCamera.viewPosition - hitPos);
    return shadePhong(hitPos, hitN, hitColor, V_hit, hitMaterial.metalness, hitMaterial.roughness);
}

[shader("vertex")]
VertexOutput vertMain(VsInput input) {
    VertexOutput o;
    float4 worldPos = mul(gModel.model, float4(input.position, 1.0));
    o.position = mul(gCamera.viewProj, worldPos);
    o.uv = input.uv;
    o.normal = normalize(mul((float3x3)gModel.model, input.normal));
    o.worldPos = worldPos.xyz;
    return o;
}

[shader("fragment")]
float4 fragMain(VertexOutput v) : SV_Target {
    float3 baseColor = gModel.albedo.rgb;
    MaterialGpu drawMaterial;
    drawMaterial.albedo = float4(1.0, 1.0, 1.0, 1.0);
    drawMaterial.textureIndex = 0;
    drawMaterial.hasTexture = 0;
    drawMaterial.metalness = 0.0;
    drawMaterial.roughness = 0.5;
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
    float3 V = normalize(gCamera.viewPosition - v.worldPos);
    float3 litColor = shadePhong(v.worldPos, N, baseColor, V, drawMaterial.metalness, drawMaterial.roughness);
    float reflectivity = drawMaterial.metalness * (1.0 - drawMaterial.roughness);
    if (reflectivity > 0.0 && gCamera.reflections_enabled != 0) {
        float3 reflected = sampleReflectionColor(v.worldPos, N, V);
        litColor = lerp(litColor, reflected, reflectivity);
    }
    return float4(litColor, 1.0f);
}
