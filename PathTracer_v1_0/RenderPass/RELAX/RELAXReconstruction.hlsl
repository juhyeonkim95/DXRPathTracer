Texture2D gDiffuseIlumination : register(t0);
Texture2D gSpecularRadiance : register(t1);
Texture2D gDiffuseReflectance : register(t2);
Texture2D gSpecularReflectance : register(t3);
Texture2D gEmission : register(t4);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);
    float3 diffuseIlumination = gDiffuseIlumination.Load(int3(ipos, 0)).rgb;
    float3 specularIllumination = gSpecularRadiance.Load(int3(ipos, 0)).rgb;
    float3 diffuseReflectance = gDiffuseReflectance.Load(int3(ipos, 0)).rgb;
    float3 specularReflectance = gSpecularReflectance.Load(int3(ipos, 0)).rgb;

    float3 emission = gEmission.Load(int3(ipos, 0)).rgb;
    float3 color = diffuseReflectance * diffuseIlumination + specularReflectance * specularIllumination + emission;

    return float4(color, 1.0f);
}