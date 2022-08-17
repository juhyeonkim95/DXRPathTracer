Texture2D gNormalDepth : register(t0);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float2 main(VS_OUTPUT input) : SV_Target
{
    float depth = gNormalDepth.Sample(s1, input.texCoord).a;
    return float2(ddx(depth), ddy(depth));
}
