struct CameraUbo {
    float4x4 viewProj;
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
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraUbo> gCamera;

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
    return o;
}

[shader("fragment")]
float4 fragMain(VertexOutput v) : SV_Target {
    if (gModel.hasTexture != 0) {
        float4 tex = gAlbedoTexture.Sample(gAlbedoSampler, v.uv);
        return float4(tex.rgb, 1.0f);
    }
    return float4(gModel.albedo.rgb, 1.0f);
}
