Texture2D col_history : register(t0);
Texture2D motion_vectors : register(t1);
Texture2D current_color : register(t2);
Texture2D moments_history : register(t3);

SamplerState s1 : register(s0);

float luma(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
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

    float new_luma = luma(col);
    float2 new_moments = float2(new_luma, new_luma * new_luma);

    float alpha = max(float(!consistency), 0.2);

    PS_OUT output;
    output.moment = lerp(moment_prev, new_moments, alpha);

    float mu_1 = output.moment.x;
    float mu_2 = output.moment.y;

    float variance = abs(mu_2 - mu_1 * mu_1);

    output.color = float4(lerp(col_prev, col, alpha), variance);
    return output;
}