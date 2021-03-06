#ifndef SHADER_STRUCTS_HLSHI
#define SHADER_STRUCTS_HLSHI

#define USE_NEXT_EVENT_ESTIMATION 1
#define USE_RUSSIAN_ROULETTE 0

#define PATHTRACE_MAX_DEPTH 8
#define PATHTRACE_RR_BEGIN_DEPTH 4

#define PATHTRACE_SPP 1

#define DO_FILTERING 1

static const float M_PIf = 3.14159265358979323846f;
static const float M_1_PIf = 0.318309886183790671538f;

enum BSDF_TYPE : uint
{
    BSDF_TYPE_DIFFUSE = 1 << 0,
    BSDF_TYPE_DIELECTRIC = 1 << 1,
    BSDF_TYPE_ROUGH_DIELECTRIC = 1 << 2,
    BSDF_TYPE_CONDUCTOR = 1 << 3,
    BSDF_TYPE_ROUGH_CONDUCTOR = 1 << 4,
    BSDF_TYPE_PLASTIC = 1 << 5,
    BSDF_TYPE_ROUGH_PLASTIC = 1 << 6,

    BSDF_TYPE_GLOSSY = BSDF_TYPE_DIFFUSE | BSDF_TYPE_ROUGH_DIELECTRIC | BSDF_TYPE_ROUGH_CONDUCTOR | BSDF_TYPE_PLASTIC | BSDF_TYPE_ROUGH_PLASTIC,
    BSDF_TYPE_DELTA = BSDF_TYPE_DIELECTRIC | BSDF_TYPE_CONDUCTOR,
    BSDF_REFLECTION = BSDF_TYPE_DIFFUSE | BSDF_TYPE_ROUGH_DIELECTRIC | BSDF_TYPE_CONDUCTOR | BSDF_TYPE_PLASTIC | BSDF_TYPE_ROUGH_PLASTIC,
    BSDF_TRANSMISSION = BSDF_TYPE_DIELECTRIC | BSDF_TYPE_ROUGH_DIELECTRIC
};

static const float SCENE_T_MIN = 1e-5;
static const float SCENE_T_MAX = 1e5;

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

struct TextureParameter
{
    uint type;
    uint id;
    float3 color0;
    float3 color1;
};

struct BSDFSample
{
    float3 wo;
    float3 weight;
    float pdf;
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
    uint frameNumber;
    uint totalFrameNumber;
    uint lightNumber;
    uint renderMode;
    float4x4 envMapTransform;
    float4x4 previousProjView;
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
    float3 normal;
    float3 Li;
    float pdf;
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
    float unused;
};

struct ShadowPayload
{
    bool hit;
};

struct Onb
{
    float3 m_tangent;
    float3 m_binormal;
    float3 m_normal;
};

struct PathTraceResult
{
    float3 radiance;
    float3 normal;
    float depth;
    uint instanceIndex;
    float3 position;
    float3 direct;
    float3 indirect;
    float3 reflectance;
};

RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

RWTexture2D<float4> gOutputHDR : register(u1);
RWTexture2D<float4> gDirectIllumination : register(u2);
RWTexture2D<float4> gIndirectIllumination : register(u3);
RWTexture2D<float4> gReflectance : register(u4);
RWTexture2D<float4> gOutputPositionGeomID : register(u5);
RWTexture2D<float4> gOutputNormal : register(u6);

//RWTexture2D<float4> gOutputHDR[4] : register(u1);
//RWTexture2D<float2> gOutputMoment[4] : register(u5);
//RWTexture2D<float4> gOutputNormal[2] : register(u9);
//RWTexture2D<float4> gOutputPositionGeomID[2] : register(u11);
//RWTexture2D<float> gOutputDepth[2] : register(u13);

//RWTexture2D<float4> gOutputHDR : register(u1);
//RWTexture2D<float4> gOutputHDR2 : register(u2);
//RWTexture2D<float4> gOutputNormal : register(u3);
//RWTexture2D<float4> gOutputNormal2 : register(u4);
//RWTexture2D<float> gOutputDepth : register(u5);
//RWTexture2D<uint> gOutputGeomID : register(u6);
//RWTexture2D<uint> gOutputGeomID2 : register(u7);
//RWTexture2D<float2> gOutputMoment : register(u8);
//RWTexture2D<float2> gOutputMoment2 : register(u9);
//RWTexture2D<uint> gAccumCountBuffer : register(u10);
//
//RWTexture2D<float4> gOutputHDRIndirect : register(u11);
//RWTexture2D<float4> gOutputHDR2Indirect : register(u12);
//RWTexture2D<float2> gOutputMomentIndirect : register(u13);
//RWTexture2D<float2> gOutputMoment2Indirect : register(u14);
//
//RWTexture2D<float4> gTest[2] : register(u15);

StructuredBuffer<Material> g_materialinfo : register(t1);
StructuredBuffer<GeometryInfo> g_geometryinfo : register(t2);
Buffer<uint> g_indices : register(t3);
StructuredBuffer<MeshVertex> g_attributes : register(t4);
// global env texture
Texture2D g_envtexture : register(t5);

// per instance
Texture2D g_textures : register(t6);



SamplerState g_s0 : register(s0);

cbuffer PerFrame : register(b0)
{
    PerFrameData g_frameData;
}

cbuffer Lights : register(b1)
{
    LightParameter lights[20];
}
#endif