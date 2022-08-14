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
Texture2D gResidualRadiance : register(t11);


SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
	uint enableDiffuseRadiance;
	uint enableDiffuseReflectance;
	uint enableSpecularRadiance;
	uint enableSpecularReflectance;
	uint enableEmission;

	uint enableDeltaReflectionRadiance;
	uint enableDeltaReflectionReflectance;
	uint enableDeltaReflectionEmission;

	uint enableDeltaTransmissionRadiance;
	uint enableDeltaTransmissionReflectance;
	uint enableDeltaTransmissionEmission;

	uint enableResidualRadiance;
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
    
    float3 residualRadiance = gResidualRadiance.Load(int3(ipos, 0)).rgb;

    float3 color = 0.0f;
    if (enableDiffuseRadiance) {
        float3 diffuse = diffuseRadiance;
        if (enableDiffuseReflectance) {
            diffuse *= diffuseReflectance;
        }
        color += diffuse;
    }

    if (enableSpecularRadiance) {
        float3 specular = specularRadiance;
        if (enableSpecularReflectance) {
            specular *= specularReflectance;
        }
        color += specular;
    }

    if (enableEmission)
    {
        color += emission;
    }

    if (enableDeltaReflectionRadiance) {
        float3 deltaReflection = deltaReflectionRadiance;
        if (enableDeltaReflectionReflectance) {
            deltaReflection *= deltaReflectionReflectance;
        }
        color += deltaReflection;
    }

    if (enableDeltaTransmissionRadiance) {
        float3 deltaTransmission = deltaTransmissionRadiance;
        if (enableDeltaTransmissionReflectance) {
            deltaTransmission *= deltaTransmissionReflectance;
        }
        color += deltaTransmission;
    }

    if (enableDeltaReflectionEmission)
    {
        color += deltaReflectionEmission;
    }

    if (enableDeltaTransmissionEmission)
    {
        color += deltaTransmissionEmission;
    }

    if (enableResidualRadiance)
    {
        // color += residualRadiance;
    }

    return float4(color, 1.0f);
}