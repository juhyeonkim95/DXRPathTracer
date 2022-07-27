Texture2D gIllumination : register(t0);
Texture2D gMoments : register(t1);
Texture2D gHistoryLength : register(t2);
Texture2D gNormalDepth : register(t3);
Texture2D gDepthDerivative : register(t3);

SamplerState s1 : register(s0);

static const float   gPhiColor = 10.0f;
static const float   gPhiNormal = 128.0f;

float luminance(float3 c) {
    return c.x * 0.2126 + c.y * 0.7152 + c.z * 0.0722;
}

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    float historyLength = gHistoryLength.Sample(s1, uv).x;
    float2 texelSize = float();

    if (historyLength < 4.0) {
        float sumWIllumination = 0.0;
        float3 sumIllumination = float3(0, 0, 0);
        float2 sumMoments = float2(0, 0);

        const float4 illuminationCenter = gIllumination.Sample(s1, uv);
        const float luminanceCenter = luminance(illuminationCenter.rgb);

        const float zCenter = gNormalDepth.Sample(s1, uv).w;
        const float2 zDerivativeDxDy = gDepthDerivative.Sample(s1, uv).rg;
        const float zDerivative = max(abs(zDerivativeDxDy.x), abs(zDerivativeDxDy.y));

        if (zCenter < 0) {
            return illuminationCenter;
        }

        const float3 nCenter = gNormalDepth.Sample(s1, uv).rgb;
        const float phiLIllumination = gPhiColor;
        const float phiDepth = max(zDerivative, 1e-8) * 3.0;

        const int radius = 3;
        for (int yy = -radius; yy <= radius; yy++) {
            for (int xx = -radius; xx <= radius; xx++) {
                const int2 p = uv + texelSize * float2(xx, yy);
                const bool inside = p.x >= 0.0 && p.y >= 0.0 && p.x <= 1.0 && p.y <= 1.0;
                const bool samePixel = (xx == 0 && yy == 0);
                const float kernel = 1.0f;
                if (inside) {
                    const float3 illuminationP = gIllumination.Sample(s1, p).rgb;
                    const float2 momentsP = gMoments.Sample(s1, p).rgb;
                    const float3 luminanceP = luminance(illuminationP);
                    const float zP = gNormalDepth.Sample(s1, p).w;
                    const float nP = gNormalDepth.Sample(s1, p).rgb;

                }
            }
        }
    }

}