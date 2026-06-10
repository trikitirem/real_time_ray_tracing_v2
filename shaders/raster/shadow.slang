struct ShadowPushConstant {
    float4x4 lightSpaceMatrix;
    float4x4 model;
};

struct ShadowInstancedPushConstant {
    float4x4 lightSpaceMatrix;
};

struct VsInput {
    float3 position : POSITION;
};

struct VsInputInstanced {
    [[vk::location(0)]] float3 position;
    [[vk::location(3)]] float4 instanceRow0;
    [[vk::location(4)]] float4 instanceRow1;
    [[vk::location(5)]] float4 instanceRow2;
    [[vk::location(6)]] float4 instanceRow3;
};

[[vk::push_constant]]
ShadowPushConstant gShadow;

[[vk::push_constant]]
ShadowInstancedPushConstant gShadowInstanced;

float4x4 loadInstanceModel(VsInputInstanced input) {
    return transpose(float4x4(input.instanceRow0,
                              input.instanceRow1,
                              input.instanceRow2,
                              input.instanceRow3));
}

[shader("vertex")]
float4 vertMain(VsInput input) : SV_Position {
    float4 world = mul(gShadow.model, float4(input.position, 1.0));
    return mul(gShadow.lightSpaceMatrix, world);
}

[shader("vertex")]
float4 vertMainInstanced(VsInputInstanced input) : SV_Position {
    float4x4 model = loadInstanceModel(input);
    float4 world = mul(model, float4(input.position, 1.0));
    return mul(gShadowInstanced.lightSpaceMatrix, world);
}

[shader("fragment")]
void fragMain() {
}
