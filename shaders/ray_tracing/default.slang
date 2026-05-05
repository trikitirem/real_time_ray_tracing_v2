struct VertexOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexID : SV_VertexID) {
    VertexOutput o;
    float x = -1.0 + float((vertexID << 1) & 2);
    float y = -1.0 + float(vertexID & 2);
    o.position = float4(x, y, 0.0, 1.0);
    return o;
}

[shader("fragment")]
float4 fragMain(VertexOutput v) : SV_Target {
    return float4(0.2f, 0.6f, 1.0f, 1.0f);
}
