#include "RELAXSpecularCommon.hlsli"

Texture2D col_history : register(t0);
Texture2D motion_vectors : register(t1);
Texture2D current_color : register(t2);
Texture2D moments_history : register(t3);
Texture2D length_history : register(t4);
Texture2D motion_vectors_delta : register(t5);
Texture2D<float> gRoughness : register(t6);

SamplerState s1 : register(s0);

//static const float   gAlpha = 0.05f;
//static const float   gMomentsAlpha = 0.2f;


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float4 color: SV_Target0;
    float2 moment: SV_Target1;
};

PS_OUT main(VS_OUTPUT input) : SV_Target
{
    const int2 ipos = int2(input.pos.xy);

    // float2 uv = input.texCoord;
    // float2 prev_uv = motion_vectors.Sample(s1, uv).rg;
    float2 prev_uv = motion_vectors.Load(int3(ipos, 0)).rg;

    prev_uv.x *= 2;
    float consistency = floor(prev_uv.x);
    prev_uv.x -= consistency;

    //float2 prev_uv_delta = motion_vectors_delta.Load(int3(ipos, 0)).rg;

    //prev_uv_delta.x *= 2;
    //prev_uv_delta.x -= floor(prev_uv_delta.x);
    //float roughness = gRoughness.Load(int3(ipos, 0)).r;

    // prev_uv = lerp(prev_uv_delta, prev_uv, clamp(roughness * 9, 0, 1));

    // float consistency = motion_vectors.Sample(s1, uv).a;
    float3 col = current_color.Load(int3(ipos, 0)).rgb;

    // float3 col = current_color.Sample(s1, uv).rgb;
    float3 col_prev = col_history.Sample(s1, prev_uv).rgb;
    float2 moment_prev = moments_history.Sample(s1, prev_uv).rg;
    float historyLength = length_history.Load(int3(ipos, 0)).x;
    // bool success = historyLength > 1.0f;
    bool success = consistency >= 1.0;

    // this adjusts the alpha for the case where insufficient history is available.
    // It boosts the temporal accumulation to give the samples equal weights in
    // the beginning.
    //const float alpha = max(gAlpha, 1.0 / historyLength);
    //const float alphaMoments = max(gMomentsAlpha, 1.0 / historyLength);

    //const float alpha = max(1 - historyLength, 0.2f);
    //const float alphaMoments = max(1 - historyLength, 0.2f);

    const float alpha = success ? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;
    const float alphaMoments = success ? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;

    //const float alpha = success ? 1.0 / historyLength : 1.0f;
    //const float alphaMoments = success ? 1.0 / historyLength : 1.0f;

    //const float alpha =  max(gAlpha, 1.0 / historyLength);
    //const float alphaMoments =  max(gMomentsAlpha, 1.0 / historyLength);

    // compute first two moments of luminance
    float new_luma = luma(col);
    float2 moments = float2(new_luma, new_luma * new_luma);

    // temporal integration of the moments
    moments = lerp(moment_prev, moments, alphaMoments);

    float variance = max(0.f, moments.g - moments.r * moments.r);

    PS_OUT output;
    // temporal integration of illumination
    //output.color = float4(float(!success), 0, 0, 1);
    //output.color = float4(float(historyLength == 1.0), 0, 0, 1);
    output.color = float4(lerp(col_prev, col, alpha), variance);
    output.moment = moments;
    return output;
}