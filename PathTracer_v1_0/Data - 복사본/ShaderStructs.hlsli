#ifndef SHADER_STRUCTS_HLSHI
#define SHADER_STRUCTS_HLSHI

#define USE_NEXT_EVENT_ESTIMATION 1
#define USE_RUSSIAN_ROULETTE 1

#define PATHTRACE_MAX_DEPTH 8
#define PATHTRACE_RR_BEGIN_DEPTH 4

#define PATHTRACE_SPP 4

static const float M_PIf = 3.14159265358979323846f;
static const float M_1_PIf = 0.318309886183790671538f;

enum BSDF_TYPE : uint
{
    BSDF_TYPE_DIFFUSE = 0,
    BSDF_TYPE_DIELECTRIC = 1,
    BSDF_TYPE_ROUGH_DIELECTRIC = 2,
    BSDF_TYPE_CONDUCTOR = 3,
    BSDF_TYPE_ROUGH_CONDUCTOR = 4,
    BSDF_TYPE_PLASTIC = 5,
    BSDF_TYPE_ROUGH_PLASTIC = 6
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
    uint lightNumber;
    float4x4 envMapTransform;
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
    float3 wi;

    float3 result;
    uint depth;

    float2 uv;

    int done;
    uint seed;
    float scatterPdf;
    uint materialIndex;
    int lightIndex;
    float t;
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


RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

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