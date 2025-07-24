cbuffer Constants : register(b0) {
    float2 offset;
    float parallaxIntensity;
    float use3DParallax;   // 0 = 2D

Texture2D topTexture : register(t0);
Texture2D midTexture : register(t1);
Texture2D baseTexture : register(t2);
Texture2D depthTexture : register(t3);
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
    output.pos = float4(input.pos, 0.0f, 1.0f);


    float zoomFactor = 0.8f;
    float2 centeredUV = input.uv - 0.5f;
    centeredUV = centeredUV * zoomFactor;
    output.uv = centeredUV + 0.5f;

    return output;
}

float4 PSmain(VertexOut input) : SV_Target {
    // Base UV coordinates
    float2 uv = input.uv;


    float backLayerOffset = 0.1f;
    float midLayerOffset = 0.3f;
    float frontLayerOffset = 0.5f;

    if (use3DParallax < 0.5f) {

        float4 baseColor = baseTexture.Sample(textureSampler, uv + offset * backLayerOffset);
        float4 midColor = midTexture.Sample(textureSampler, uv + offset * midLayerOffset);
        float4 topColor = topTexture.Sample(textureSampler, uv + offset * frontLayerOffset);

        float4 finalColor = baseColor;
        // TRANSPARENCY, LAYER MIXING
        finalColor = lerp(finalColor, midColor, 0.6f);
        finalColor = lerp(finalColor, topColor, 0.3f);

        finalColor.a = 1.0f;

        return finalColor;
    }
    else {
        float depth = depthTexture.Sample(textureSampler, uv).r;

        float depthFactor = 1.0 - depth; // INVERTED

        float2 offsetUV = uv + offset * depthFactor * parallaxIntensity;

        float4 baseColor = baseTexture.Sample(textureSampler, uv + offset * backLayerOffset * (1.0 - depthFactor));
        float4 midColor = midTexture.Sample(textureSampler, uv + offset * midLayerOffset * depthFactor);
        float4 topColor = topTexture.Sample(textureSampler, uv + offset * frontLayerOffset * depthFactor);

        float4 finalColor = baseColor;
        finalColor = lerp(finalColor, midColor, 0.6f);
        finalColor = lerp(finalColor, topColor, 0.3f);
        finalColor.a = 1.0f;

        return finalColor;
    }
}