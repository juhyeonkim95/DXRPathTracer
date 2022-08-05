#include "../Common/ColorHelpers.hlsli"
#include "TonemapParams.hlsli"

Texture2D t1 : register(t0);

SamplerState s1 : register(s0);

cbuffer PerImageCB : register(b0)
{
    ToneMapperParams gParams;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

// Linear
float3 toneMapLinear(float3 color)
{
    return color;
}

// Reinhard
float3 toneMapReinhard(in float3 color)
{
    float luma = luminance(color);
    float reinhard = luma / (luma + 1);
    return color * (reinhard / luma);
}

// Reinhard with maximum luminance
float3 toneMapReinhardModified(float3 color)
{
    float luma = luminance(color);
    float reinhard = luma * (1 + luma / (gParams.whiteMaxLuminance * gParams.whiteMaxLuminance)) * (1 + luma);
    return color * (reinhard / luma);
}

// John Hable's ALU approximation of Jim Heji's operator
// http://filmicgames.com/archives/75
float3 toneMapHejiHableAlu(float3 color)
{
    color = max(float(0).rrr, color - 0.004);
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);

    // Result includes sRGB conversion
    return pow(color, 2.2);
}


// John Hable's Uncharted 2 filmic tone map
// http://filmicgames.com/archives/75
float3 applyUc2Curve(float3 color)
{
    float A = 0.22; // Shoulder Strength
    float B = 0.3;  // Linear Strength
    float C = 0.1;  // Linear Angle
    float D = 0.2;  // Toe Strength
    float E = 0.01; // Toe Numerator
    float F = 0.3;  // Toe Denominator

    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - (E / F);
    return color;
}

float3 toneMapHableUc2(float3 color)
{
    float exposureBias = 2.0f;
    color = applyUc2Curve(exposureBias * color);
    float whiteScale = 1 / applyUc2Curve(float3(gParams.whiteScale, gParams.whiteScale, gParams.whiteScale)).x;
    color = color * whiteScale;

    return color;
}

float3 toneMapAces(float3 color)
{
    // Cancel out the pre-exposure mentioned in
    // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    color *= 0.6;

    float A = 2.51;
    float B = 0.03;
    float C = 2.43;
    float D = 0.59;
    float E = 0.14;

    color = saturate((color * (A * color + B)) / (color * (C * color + D) + E));
    return color;
}

float3 toneMap(float3 color)
{
    switch (gParams.mode)
    {
    case ToneMapperOperator::Linear:
        return toneMapLinear(color);
    case ToneMapperOperator::Reinhard:
        return toneMapReinhard(color);
    case ToneMapperOperator::ReinhardModified:
        return toneMapReinhardModified(color);
    case ToneMapperOperator::HejiHableAlu:
        return toneMapHejiHableAlu(color);
    case ToneMapperOperator::HableUc2:
        return toneMapHableUc2(color);
    case ToneMapperOperator::Aces:
        return toneMapAces(color);
    default:
        return color;
    }
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 color = t1.Sample(s1, input.texCoord).rgb;

    color = toneMap(color);
    
    color = gParams.srgbConversion ? linearToSRGB(color) : color;

    return float4(color, 1.0f);
}