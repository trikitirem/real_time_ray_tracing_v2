struct CameraUbo {
    float4x4 viewProj;
    float3 viewPosition;
    uint _pad0;
};

struct LightUbo {
    float4x4 lightSpaceMatrix;
    float3 lightDirToLight;
    float _pad0;
};

struct ModelPushConstant {
    float4x4 model;
    float4 albedo;
    uint hasTexture;
    float roughness;
    uint _pad0;
    uint _pad1;
};

struct VsInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct VsInputInstanced {
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float2 uv;
    [[vk::location(2)]] float3 normal;
    [[vk::location(3)]] float4 instanceRow0;
    [[vk::location(4)]] float4 instanceRow1;
    [[vk::location(5)]] float4 instanceRow2;
    [[vk::location(6)]] float4 instanceRow3;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float4 lightSpacePos : TEXCOORD3;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraUbo> gCamera;

[[vk::push_constant]]
ModelPushConstant gModel;

struct InstancedBatchPushConstant {
    float4 albedo;
    uint   hasTexture;
    float  roughness;
    uint   _pad0;
    uint   _pad1;
};
[[vk::push_constant]]
InstancedBatchPushConstant gBatch;

[[vk::binding(0, 1)]]
Texture2D<float4> gAlbedoTexture;

[[vk::binding(1, 1)]]
SamplerState gAlbedoSampler;

[[vk::binding(0, 2)]]
ConstantBuffer<LightUbo> gLight;

[[vk::binding(1, 2)]]
Texture2D<float> gShadowMap;

[[vk::binding(2, 2)]]
SamplerComparisonState gShadowSampler;

[shader("vertex")]
VertexOutput vertMain(VsInput input) {
    VertexOutput o;
    float4 worldPos = mul(gModel.model, float4(input.position, 1.0));
    o.position = mul(gCamera.viewProj, worldPos);
    o.uv = input.uv;
    o.normal = normalize(mul((float3x3)gModel.model, input.normal));
    o.worldPos = worldPos.xyz;
    o.lightSpacePos = mul(gLight.lightSpaceMatrix, worldPos);
    return o;
}

float4x4 loadInstanceModel(VsInputInstanced input) {
    // GLM stores column-major; vec4 attributes arrive as columns, not rows.
    // Constructing float4x4 from 4 float4s in HLSL/Slang treats them as rows,
    // so we must transpose to recover the original column-major transform.
    return transpose(float4x4(input.instanceRow0,
                              input.instanceRow1,
                              input.instanceRow2,
                              input.instanceRow3));
}

float sampleShadow(float4 lightSpacePos) {
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    float2 uv = projCoords.xy * 0.5 + 0.5;
    
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || projCoords.z > 1.0)
        return 1.0;
    
    return gShadowMap.SampleCmpLevelZero(gShadowSampler, uv, projCoords.z);
}

[shader("fragment")]
float4 fragMain(VertexOutput v) : SV_Target {
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
    float3 L = normalize(gLight.lightDirToLight);
    float3 V = normalize(gCamera.viewPosition - v.worldPos);
    float3 H = normalize(L + V);

    float3 ambient = ambientStrength * lightColor * baseColor;
    float ndotl = saturate(dot(N, L));
    float shadow = sampleShadow(v.lightSpacePos);
    
    float3 diffuse = shadow * ndotl * lightColor * baseColor;
    float spec = pow(saturate(dot(N, H)), shininess);
    float3 specular = shadow * specularStrength * (1.0 - gModel.roughness) * spec * lightColor;

    return float4(ambient + diffuse + specular, 1.0f);
}

[shader("vertex")]
VertexOutput vertMainInstanced(VsInputInstanced input) {
    VertexOutput o;
    float4x4 model = loadInstanceModel(input);
    float4 worldPos = mul(model, float4(input.position, 1.0));
    o.position = mul(gCamera.viewProj, worldPos);
    o.uv = input.uv;
    o.normal = normalize(mul((float3x3)model, input.normal));
    o.worldPos = worldPos.xyz;
    o.lightSpacePos = mul(gLight.lightSpaceMatrix, worldPos);
    return o;
}

[shader("fragment")]
float4 fragMainInstanced(VertexOutput v) : SV_Target {
    const float3 lightColor = float3(1.0, 1.0, 1.0);
    const float ambientStrength = 0.12;
    const float specularStrength = 0.7;
    const float shininess = 24.0;

    float3 baseColor = gBatch.albedo.rgb;
    if (gBatch.hasTexture != 0) {
        float4 tex = gAlbedoTexture.Sample(gAlbedoSampler, v.uv);
        baseColor = tex.rgb;
    }

    float3 N = normalize(v.normal);
    float3 L = normalize(gLight.lightDirToLight);
    float3 V = normalize(gCamera.viewPosition - v.worldPos);
    float3 H = normalize(L + V);

    float3 ambient = ambientStrength * lightColor * baseColor;
    float ndotl = saturate(dot(N, L));
    float shadow = sampleShadow(v.lightSpacePos);

    float3 diffuse = shadow * ndotl * lightColor * baseColor;
    float spec = pow(saturate(dot(N, H)), shininess);
    float3 specular = shadow * specularStrength * (1.0 - gBatch.roughness) * spec * lightColor;

    return float4(ambient + diffuse + specular, 1.0f);
}
