#ifndef COMMON_STRUCTS
#define COMMON_STRUCTS

struct Material
{
    uint materialType;
    uint isDoubleSided;

    float3 diffuseReflectance;
    uint diffuseReflectanceTextureID;
    float3 specularReflectance;
    uint specularReflectanceTextureID;
    float3 specularTransmittance;
    uint specularTransmittanceTextureID;

    float3 eta;
    float3 k;

    float intIOR;
    float extIOR;

    uint microfacetDistribution;
    float roughness;

    uint nonlinear;
    uint unused;
};



struct BSDFSample
{
    float3 wo;
    float3 weight;
    float pdf;
    uint sampledLobe;
};

struct GeometryInfo
{
    uint materialIndex;
    int lightIndex;
    uint indicesOffset;
    uint verticesOffset;
};

struct MeshVertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct PerFrameData
{
    float4 u;
    float4 v;
    float4 w;
    float4 cameraPosition;

    float4x4 envMapTransform;
    float4x4 previousProjView;
    float4 previousCameraPosition;

    uint frameNumber;
    uint totalFrameNumber;
    uint lightNumber;
    uint renderMode;

    uint cameraChanged;
    uint paramChanged;
    uint unused1;
    uint unused2;
};

struct LightParameter
{
    float4 position;
    float4 u;
    float4 v;
    float4 emission;
    float4 normalAndPdf;
};

struct LightSample
{
    float3 position;
    float pdf;
    float3 normal;
    uint lightIndex;
    float3 Li;
    uint unused;
};

struct Reservoir
{
    float wSum;
    float W;
    float M;
    float unused;
    LightSample lightSample;
};


struct SurfaceInteraction
{
    float3 p;
    float3 wi;
    float3 emission;
    float3 normal;

    float2 uv;
    uint materialIndex;
    int lightIndex;
    float t;

    int done;
    uint seed;
    float scatterPdf;
};


struct RayPayload
{
    float3 emission;
    float3 attenuation;

    float3 diffuseReflectance;
    float3 specularReflectance;
    float3 specularTransmittance;
    float roughness;

    float3 direction;
    float3 origin;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float3 wi;

    float3 result;
    uint depth;

    float2 uv;

    int done;
    uint seed;
    float scatterPdf;
    uint instanceIndex;
    uint materialIndex;
    int lightIndex;
    float t;
    // float unused;

    uint sampledLobe;
    uint requestedLobe;
    uint primaryReflectionLobe;
};

struct ShadowPayload
{
    bool hit;
};

struct PathTraceResult
{
    float3 radiance;
    float3 normal;
    float depth;
    uint instanceIndex;
    float3 position;

    float3 emission;
    float3 directRadiance;
    float3 indirectRadiance;
    float3 diffuseRadiance;
    float3 specularRadiance;

    float3 deltaReflectionReflectance;
    float3 deltaReflectionEmission;
    float3 deltaReflectionRadiance;

    float3 deltaTransmissionReflectance;
    float3 deltaTransmissionEmission;
    float3 deltaTransmissionRadiance;

    float3 deltaReflectionPosition;
    float3 deltaReflectionNormal;
    uint deltaReflectionMeshID;

    float3 deltaTransmissionPosition;
    float3 deltaTransmissionNormal;
    uint deltaTransmissionMeshID;

    float3 residualRadiance;

    uint primaryPathType;

    float3 reflectance;
    float3 diffuseReflectance;
    float3 specularReflectance;
    float3 indirectReflectance;

    uint pathType;
};

struct TextureParameter
{
    uint type;
    uint id;
    float3 color0;
    float3 color1;
};


struct Onb
{
    float3 m_tangent;
    float3 m_binormal;
    float3 m_normal;
};

struct PathTracerParams
{
    int maxDepth;
    int maxDepthDiffuse;
    int maxDepthSpecular;
    int maxDepthTransmittance;

    bool accumulateFrames;
    int unused1;
    int unused2;
    int unused3;
};

struct ReSTIRParams
{
    int enableReSTIR;
    int resamplingMode;
    int lightCandidateCount;
    int maxHistoryLength;
    float normalThreshold;
    float depthThreshold;
};

#endif