Texture2D gColorVariance : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPositionMeshID : register(t2);

static const float sigmaP = 1.0;
static const float sigmaN = 128.0;
static const float sigmaL = 4.0;

static const float epsilon = 0.00001;

float calculateWeight(in float3 p1, in float3 p2, in float3 n1, in float3 n2, float l1, float l2, float customSigmaL)
{
	float wpPow = -length(p1 - p2) / sigmaP;
	float wn = pow(max(0.0, dot(n1, n2)), sigmaN);
	float wlPow = -abs(l1 - l2) / customSigmaL;
	return wn * exp(wpPow + wlPow);
}