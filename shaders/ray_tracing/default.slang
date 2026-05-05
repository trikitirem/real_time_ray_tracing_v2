struct CameraUbo {
    float4x4 viewProj;
    float3 viewPosition;
    uint _pad0;
};

struct ModelPushConstant {
    float4x4 model;
    float4 albedo;
    uint hasTexture;
    uint _pad0;
    uint _pad1;
    uint _pad2;
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

[[vk::push_constant]]
ModelPushConstant gModel;

[[vk::binding(0, 1)]]
Texture2D<float4> gAlbedoTexture;

[[vk::binding(1, 1)]]
SamplerState gAlbedoSampler;

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
    const float3 lightDirToLight = normalize(float3(0.7, 1.0, 0.2));
    const float3 lightColor = float3(1.0, 1.0, 1.0);
    const float ambientStrength = 0.12;
    const float specularStrength = 0.7;
    const float shininess = 24.0;

    float3 baseColor = gModel.albedo.rgb;
    if (gModel.hasTexture != 0) {
        float4 tex = gAlbedoTexture.Sample(gAlbedoSampler, v.uv);
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
    return float4(litColor, 1.0f);
}
