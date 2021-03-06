Texture2D col_history : register(t0);
Texture2D motion_vectors : register(t1);
Texture2D current_color : register(t2);
Texture2D moments_history : register(t3);
Texture2D length_history : register(t4);

SamplerState s1 : register(s0);

static const float   gAlpha = 0.05f;
static const float   gMomentsAlpha = 0.2f;

float luminance(float3 c) {
    return c.x * 0.2126 + c.y * 0.7152 + c.z * 0.0722;
}

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
    float2 uv = input.texCoord;
    float2 prev_uv = motion_vectors.Sample(s1, uv).rg;

    float consistency = motion_vectors.Sample(s1, uv).a;

    float3 col = current_color.Sample(s1, uv).rgb;
    float3 col_prev = col_history.Sample(s1, prev_uv).rgb;

    float2 moment_prev = moments_history.Sample(s1, prev_uv).rg;
    
    float historyLength = length_history.Sample(s1, uv).x;

    bool success = consistency == 1.0;
    
    // this adjusts the alpha for the case where insufficient history is available.
    // It boosts the temporal accumulation to give the samples equal weights in
    // the beginning.
    //const float alpha = max(gAlpha, 1.0 / historyLength);
    //const float alphaMoments = max(gMomentsAlpha, 1.0 / historyLength);

    //const float alpha = max(1 - historyLength, 0.2f);
    //const float alphaMoments = max(1 - historyLength, 0.2f);

    const float alpha = success ? max(gAlpha, 1.0 / historyLength) :1.0f;
    const float alphaMoments = success ? max(gMomentsAlpha, 1.0 / historyLength) :1.0f;
    
    //const float alpha =  max(gAlpha, 1.0 / historyLength);
    //const float alphaMoments =  max(gMomentsAlpha, 1.0 / historyLength);

    // compute first two moments of luminance
    float new_luma = luminance(col);
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