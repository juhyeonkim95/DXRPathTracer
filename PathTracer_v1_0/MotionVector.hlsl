Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormal : register(t3);
Texture2D gHistoryLength : register(t4);

SamplerState s1 : register(s0);

struct PerFrameData
{
    float4 u;
    float4 v;
    float4 w;
    float4 cameraPosition;
    uint frameNumber;
    uint totalFrameNumber;
    uint lightNumber;
    uint renderMode;
    float4x4 envMapTransform;
    float4x4 previousProjView;
};


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float2 motionVector: SV_Target0;
    float historyLength : SV_Target1;
};

cbuffer ConstantBuffer : register(b0)
{
    PerFrameData g_frameData;
};


PS_OUT main(VS_OUTPUT input) : SV_TARGET
{
    float3 position = gPositionMeshID.Sample(s1, input.texCoord).rgb;

    // Reprojection
    float4 projCoord = mul(float4(position, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    bool consistency = true;

    float3 previousNormal = gNormalPrev.Sample(s1, prevPixel).rgb;
    float3 normal = gNormal.Sample(s1, input.texCoord).rgb;

    float previousDepth = gNormalPrev.Sample(s1, prevPixel).w;
    float depth = gNormal.Sample(s1, input.texCoord).w;

    float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    float meshID = gPositionMeshID.Sample(s1, input.texCoord).w;

    // check whether reprojected pixel is inside of the screen
    if (prevPixel.x < 0 || prevPixel.x > 1 || prevPixel.y < 0 || prevPixel.y > 1) {
        consistency = false;
    }

    // check if deviation of depths is acceptable
    else if (meshID != previousMeshID) {
        consistency = false;
    }

    // reject if the normal deviation is not acceptable
    else if (distance(normal, previousNormal) > 1e-1f) {
        consistency = false;
    }

    else if (abs(depth - previousDepth) > 0.1) {
        consistency = false;
    }

    float historyLength = gHistoryLength.Sample(s1, prevPixel).r;
    historyLength = min(32.0f, consistency ? historyLength + 1.0f : 1.0f);

    PS_OUT output;
    output.motionVector = prevPixel;
    output.historyLength =  float(consistency);

    return output;
}