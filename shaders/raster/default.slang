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
    float4 lightSpacePos : TEXCOORD3;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraUbo> gCamera;

[[vk::push_constant]]
ModelPushConstant gModel;

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
    float3 specular = shadow * specularStrength * spec * lightColor;

    return float4(ambient + diffuse + specular, 1.0f);
}
