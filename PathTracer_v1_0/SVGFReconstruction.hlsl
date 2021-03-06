Texture2D direct : register(t0);
Texture2D indirect : register(t1);
Texture2D albedo : register(t2);
Texture2D original : register(t3);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 directColor = direct.Sample(s1, input.texCoord).rgb;
    float3 indirectColor = indirect.Sample(s1, input.texCoord).rgb;
    float3 albedoColor = albedo.Sample(s1, input.texCoord).rgb;
    float3 color = albedoColor * (directColor + indirectColor);

    float3 originalColor = original.Sample(s1, input.texCoord).rgb;

    //if (input.texCoord.x < 0.5) {
    //    color = originalColor;
    //}

    return float4(color, 1.0f);
}