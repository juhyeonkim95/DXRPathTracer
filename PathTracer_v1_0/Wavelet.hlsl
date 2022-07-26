Texture2D gColorVariance : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPositionMeshID : register(t2);

SamplerState s1 : register(s0);

static const int support = 2;
static const float sigmaP = 1.0;
static const float sigmaN = 128.0;
static const float sigmaL = 4.0;

static const float epsilon = 0.00001;

cbuffer ConstantBuffer : register(b0)
{
    int level;
};


static const float h[25] = { 1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0,
    1.0 / 64.0, 1.0 / 16.0, 3.0 / 32.0, 1.0 / 16.0, 1.0 / 64.0,
    3.0 / 128.0, 3.0 / 32.0, 9.0 / 64.0, 3.0 / 32.0, 3.0 / 128.0,
    1.0 / 64.0, 1.0 / 16.0, 3.0 / 32.0, 1.0 / 16.0, 1.0 / 64.0,
    1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0 };
static const float gaussKernel[9] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 };

float luma(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

bool eps_equal(float a, float b) {
    return round(a) == round(b);
}

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float4 pPositionMeshId = gPositionMeshID.Sample(s1, input.texCoord);
    float3 pPosition = pPositionMeshId.rgb;
    float pMeshID = pPositionMeshId.a;

    float3 pNormal = gNormal.Sample(s1, input.texCoord).rgb;
    float3 pColor = gColorVariance.Sample(s1, input.texCoord).rgb;
    float pLuminance = luma(pColor);

    float2 textureSize = float2(720.0f, 1280.0f);
    float2 texelSize = 1.0 / textureSize;

    int step = 1 << level;

    float3 c = float3(0.0f, 0.0f, 0.0f);
    float v = 0.0f;
    float weights = 0.0f;

    for (int offsetx = -support; offsetx <= support; offsetx++) {
        for (int offsety = -support; offsety <= support; offsety++) {
            float2 loc = input.texCoord + texelSize * float2(offsetx, offsety) * step;

            float4 qPositionMeshId = gPositionMeshID.Sample(s1, loc);
            float qMeshID = qPositionMeshId.a;

            if (eps_equal(pMeshID, qMeshID)) {
                float3 qPosition = qPositionMeshId.rgb;
                float3 qNormal = gNormal.Sample(s1, loc).rgb;

                float3 qColor = gColorVariance.Sample(s1, loc).rgb;
                float qVariance = gColorVariance.Sample(s1, loc).a;
                float qLuminance = luma(qColor);

                float3 t = pPosition - qPosition;
                float dist2 = dot(t, t) + t.z * t.z;
                float wp = min(exp(-(dist2 / sigmaP)), 1.0);

                float wn = pow(max(0.0, dot(pNormal, qNormal)), sigmaN);

                float gvl = 0.001;
                for (int y0 = -1; y0 <= 1; y0++) {
                    for (int x0 = -1; x0 <= 1; x0++) {
                        gvl += gaussKernel[x0 + 3 * y0 + 4] * gColorVariance.Sample(s1, loc + float2(x0, y0) * texelSize).a;
                    }
                }

                float wl = min(1.0, exp(-abs(pLuminance - qLuminance) / (sigmaL * sqrt(gvl) + epsilon) ));

                float w = wp * wn * wl;
                float weight = h[5 * (offsety + support) + offsetx + support] * w;

                c += weight * qColor;
                v += weight * weight * qVariance;
                weights += weight;
            }
        }
    }

    if (weights > epsilon) {
        c /= weights;
    }
    else {
        c = pColor;
    }

    return float4(c, 1.0f);
}