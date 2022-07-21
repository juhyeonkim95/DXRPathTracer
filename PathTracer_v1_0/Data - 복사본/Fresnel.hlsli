namespace fresnel
{
	float ConductorReflectance(float eta, float k, float cosThetaI) {
		float cosThetaISq = cosThetaI * cosThetaI;
		float sinThetaISq = max(1.0f - cosThetaISq, 0.0f);
		float sinThetaIQu = sinThetaISq * sinThetaISq;

		float innerTerm = eta * eta - k * k - sinThetaISq;
		float aSqPlusBSq = sqrt(max(innerTerm * innerTerm + 4.0f * eta * eta * k * k, 0.0f));
		float a = sqrt(max((aSqPlusBSq + innerTerm) * 0.5f, 0.0f));

		float Rs = ((aSqPlusBSq + cosThetaISq) - (2.0f * a * cosThetaI)) /
			((aSqPlusBSq + cosThetaISq) + (2.0f * a * cosThetaI));
		float Rp = ((cosThetaISq * aSqPlusBSq + sinThetaIQu) - (2.0f * a * cosThetaI * sinThetaISq)) /
			((cosThetaISq * aSqPlusBSq + sinThetaIQu) + (2.0f * a * cosThetaI * sinThetaISq));

		return 0.5f * (Rs + Rs * Rp);
	}

	float3 ConductorReflectance(in float3 eta, in float3 k, float cosThetaI) {
		return float3(
			ConductorReflectance(eta.x, k.x, cosThetaI),
			ConductorReflectance(eta.y, k.y, cosThetaI),
			ConductorReflectance(eta.z, k.z, cosThetaI)
		);
	}

	float DielectricReflectance(in float eta, in float cosThetaI, inout float cosThetaT) {
		if (cosThetaI < 0.0f) {
			eta = 1.0f / eta;
			cosThetaI = -cosThetaI;
		}
		float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);
		if (sinThetaTSq > 1.0f) {
			cosThetaT = 0.0f;
			return 1.0f;
		}
		cosThetaT = sqrt(max(1.0f - sinThetaTSq, 0.0f));

		float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
		float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

		return (Rs * Rs + Rp * Rp) * 0.5f;
	}
}