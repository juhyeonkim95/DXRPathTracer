static const float epsilon = 0.00001;

cbuffer ConstantBuffer : register(b0)
{
	float2 texelSize;
	float gAlpha;
	float gMomentsAlpha;
	float sigmaZ;
	float sigmaN;
	float sigmaL;
	int stepSize;
};


float luma(float3 c) {
	return c.x * 0.2126 + c.y * 0.7152 + c.z * 0.0722;
}

float calculateWeight(in float z1, in float z2, float customSigmaZ, in float3 n1, in float3 n2, float l1, float l2, float customSigmaL)
{
	float wzPow = customSigmaZ == 0.0f? 0 : -abs(z1 - z2) / customSigmaZ;
	float wn = pow(max(0.0, dot(n1, n2)), sigmaN);
	float wlPow = -abs(l1 - l2) / customSigmaL;
	return wn * exp(wzPow + wlPow);
}

float calculateWeightPosition(in float3 p1, in float3 p2, float customSigmaP, in float3 n1, in float3 n2, float l1, float l2, float customSigmaL)
{
	float wpPow = -length(p1 - p2) / customSigmaP;
	float wn = pow(max(0.0, dot(n1, n2)), sigmaN);
	float wlPow = -abs(l1 - l2) / customSigmaL;
	return wn * exp(wpPow + wlPow);
}