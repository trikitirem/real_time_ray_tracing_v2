struct CameraUbo {
    float4x4 viewProj;
};

struct ModelPushConstant {
    float4x4 model;
    float4 albedo;
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
    return float4(gModel.albedo.rgb, 1.0f);
}
