#ifndef MICROFACET
#define MICROFACET


enum MICROFACET_DISTRIBUTION_TYPE : uint
{
    MICROFACET_DISTRIBUTION_TYPE_BECKMANN = 0,
    MICROFACET_DISTRIBUTION_TYPE_GGX = 1,
    MICROFACET_DISTRIBUTION_TYPE_PHONG = 2
};

namespace microfacet
{
    float roughnessToAlpha(uint dist, float roughness) 
    {
        float minAlpha = 1e-3f;
        roughness = max(roughness, minAlpha);
        if (dist == MICROFACET_DISTRIBUTION_TYPE_PHONG)
            return 2.0f / (roughness * roughness) - 2.0f;
        return roughness;
    }

    float D(uint dist, float alpha, inout float3 m)
    {
        if (m.z <= 0.0f) {
            return 0.0f;
        }
        switch (dist) {
        case MICROFACET_DISTRIBUTION_TYPE_BECKMANN: {
            float alphaSq = alpha * alpha;
            float cosThetaSq = m.z * m.z;
            float tanThetaSq = max(1.0f - cosThetaSq, 0.0f) / cosThetaSq;
            float cosThetaQu = cosThetaSq * cosThetaSq;
            return M_1_PIf * exp(-tanThetaSq / alphaSq) / (alphaSq * cosThetaQu);
        }
        case MICROFACET_DISTRIBUTION_TYPE_PHONG:
            return (alpha + 2.0f) * M_1_PIf * 0.5f * pow(m.z, alpha);
        case MICROFACET_DISTRIBUTION_TYPE_GGX: {
            float a2 = alpha * alpha;
            float t = 1.0 + (a2 - 1.0) * m.z * m.z;
            return a2 / (M_PIf * t * t);
        }
        }
        return 0.0f;
    }

    float G1(uint dist, float alpha, in float3 v, in float3 m)
    {
        if (dot(v, m) * v.z <= 0.0f)
            return 0.0f;
        switch (dist) {
        case MICROFACET_DISTRIBUTION_TYPE_BECKMANN: {
            float cosThetaSq = v.z * v.z;
            float tanTheta = abs(sqrt(max(1.0f - cosThetaSq, 0.0f)) / v.z);
            float a = 1.0f / (alpha * tanTheta);
            if (a < 1.6f)
                return (3.535f * a + 2.181f * a * a) / (1.0f + 2.276f * a + 2.577f * a * a);
            else
                return 1.0f;
        }
        case MICROFACET_DISTRIBUTION_TYPE_PHONG: {
            float cosThetaSq = v.z * v.z;
            float tanTheta = abs(sqrt(max(1.0f - cosThetaSq, 0.0f)) / v.z);
            float a = sqrt(0.5f * alpha + 1.0f) / tanTheta;
            if (a < 1.6f)
                return (3.535f * a + 2.181f * a * a) / (1.0f + 2.276f * a + 2.577f * a * a);
            else
                return 1.0f;
        }
        case MICROFACET_DISTRIBUTION_TYPE_GGX: {
            float alphaSq = alpha * alpha;
            float cosThetaSq = v.z * v.z;
            float tanThetaSq = max(1.0f - cosThetaSq, 0.0f) / cosThetaSq;
            return 2.0f / (1.0f + sqrt(1.0f + alphaSq * tanThetaSq));
        }
        }

        return 0.0f;
    }

    float G(uint dist, float alpha, in float3 wi, in float3 wo, in float3 m)
    {
        return G1(dist, alpha, wi, m) * G1(dist, alpha, wo, m);
    }

    float Pdf(uint dist, float alpha, in float3 m)
    {
        return D(dist, alpha, m) * m.z;
    }

    float3 Sample(uint dist, float alpha, float r1, float r2)
    {
        float phi = r2 * 2 * M_PIf;
        float cosTheta = 0.0f;

        switch (dist) {
        case MICROFACET_DISTRIBUTION_TYPE_BECKMANN: {
            float tanThetaSq = -alpha * alpha * log(1.0f - r1);
            cosTheta = 1.0f / sqrt(1.0f + tanThetaSq);
            break;
        }
        case MICROFACET_DISTRIBUTION_TYPE_PHONG: {
            cosTheta = float(pow(r1, 1.0 / (alpha + 2.0)));
            break;
        }
        case MICROFACET_DISTRIBUTION_TYPE_GGX: {
            float tanThetaSq = alpha * alpha * r1 / (1.0f - r1);
            cosTheta = 1.0f / sqrt(1.0f + tanThetaSq);
            break;
        }
        }
        float r = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
        return float3(cos(phi) * r, sin(phi) * r, cosTheta);
    }


}

#endif