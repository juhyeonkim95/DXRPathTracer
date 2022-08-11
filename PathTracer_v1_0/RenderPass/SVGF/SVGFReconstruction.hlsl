Texture2D gDirect : register(t0);
Texture2D gIndirect : register(t1);
Texture2D gAlbedo : register(t2);
Texture2D gEmission : register(t3);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);
    float3 directColor = gDirect.Load(int3(ipos, 0)).rgb;
    float3 inDirectColor = gIndirect.Load(int3(ipos, 0)).rgb;
    float3 albedo = gAlbedo.Load(int3(ipos, 0)).rgb;
    float3 emission = gEmission.Load(int3(ipos, 0)).rgb;
    float3 color = albedo * (directColor + inDirectColor) + emission;
    return float4(color, 1.0f);
}