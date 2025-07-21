cbuffer Constants : register(b0) {
    float2 offset;
};

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

struct VertexIn {
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VertexOut {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VertexOut VSmain(VertexIn input) {
    VertexOut output;
    output.pos = float4(input.pos + offset, 0.0f, 1.0f);
    output.uv = input.uv + offset * 0.05f;
    return output;
}

float4 PSmain(VertexOut input) : SV_Target {
    return diffuseTexture.Sample(textureSampler, input.uv);
}