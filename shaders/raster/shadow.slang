struct ShadowPushConstant {
    float4x4 lightSpaceMatrix;
    float4x4 model;
};

struct VsInput {
    float3 position : POSITION;
};

[[vk::push_constant]]
ShadowPushConstant gShadow;

[shader("vertex")]
float4 vertMain(VsInput input) : SV_Position {
    float4 world = mul(gShadow.model, float4(input.position, 1.0));
    return mul(gShadow.lightSpaceMatrix, world);
}

[shader("fragment")]
void fragMain() {
}
