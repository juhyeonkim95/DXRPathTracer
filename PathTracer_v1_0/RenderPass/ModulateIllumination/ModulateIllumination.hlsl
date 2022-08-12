Texture2D gDiffuseRadiance : register(t0);
Texture2D gSpecularRadiance : register(t1);
Texture2D gDiffuseReflectance : register(t2);
Texture2D gSpecularReflectance : register(t3);
Texture2D gEmission : register(t4);
Texture2D gDeltaReflectionRadiance : register(t5);
Texture2D gDeltaTransmissionRadiance : register(t6);
Texture2D gDeltaReflectionReflectance : register(t7);
Texture2D gDeltaTransmissionReflectance : register(t8);
Texture2D gDeltaReflectionEmission : register(t9);
Texture2D gDeltaTransmissionEmission : register(t10);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);
    float3 diffuseRadiance = gDiffuseRadiance.Load(int3(ipos, 0)).rgb;
    float3 specularRadiance = gSpecularRadiance.Load(int3(ipos, 0)).rgb;
    float3 diffuseReflectance = gDiffuseReflectance.Load(int3(ipos, 0)).rgb;
    float3 specularReflectance = gSpecularReflectance.Load(int3(ipos, 0)).rgb;
    float3 emission = gEmission.Load(int3(ipos, 0)).rgb;

    float3 deltaReflectionRadiance = gDeltaReflectionRadiance.Load(int3(ipos, 0)).rgb;
    float3 deltaTransmissionRadiance = gDeltaTransmissionRadiance.Load(int3(ipos, 0)).rgb;
    float3 deltaReflectionReflectance = gDeltaReflectionReflectance.Load(int3(ipos, 0)).rgb;
    float3 deltaTransmissionReflectance = gDeltaTransmissionReflectance.Load(int3(ipos, 0)).rgb;
    float3 deltaReflectionEmission = gDeltaReflectionEmission.Load(int3(ipos, 0)).rgb;
    float3 deltaTransmissionEmission = gDeltaTransmissionEmission.Load(int3(ipos, 0)).rgb;

    float3 color = diffuseReflectance * diffuseRadiance + specularReflectance * specularRadiance + emission;
    color += deltaReflectionRadiance * deltaReflectionReflectance + deltaReflectionEmission;
    color += deltaTransmissionRadiance * deltaTransmissionReflectance + deltaTransmissionEmission;

    return float4(color, 1.0f);
}