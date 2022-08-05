Texture2D t1 : register(t0);
Texture2D t2 : register(t1);

SamplerState s1 : register(s0);

cbuffer PerImageCB : register(b0)
{
    float alpha;
    uint divide;
    uint unused1;
    uint unused2;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 color1 = t1.Sample(s1, input.texCoord).rgb;
    float3 color2 = t2.Sample(s1, input.texCoord).rgb;

    float3 color = lerp(color1, color2, alpha);

    return float4(color, 1.0f);
}