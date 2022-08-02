Texture2D t1 : register(t0);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 color = t1.Sample(s1, input.texCoord).rgb;

    color = color / (color + 1);
    color = pow(color, 1.f / 2.2f);
    return float4(color, 1.0f);
}