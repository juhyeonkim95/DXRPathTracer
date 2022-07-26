#pragma once
#include "Framework.h"

struct Material
{
    uint materialType;
    uint isDoubleSided;

    vec3 diffuseReflectance;
    uint diffuseReflectanceTextureID;
    vec3 specularReflectance;
    uint specularReflectanceTextureID;
    vec3 specularTransmittance;
    uint specularTransmittanceTextureID;

    vec3 eta;
    vec3 k;

    float intIOR;
    float extIOR;

    uint microfacetDistribution;
    float roughness;

    uint nonlinear;
    uint unused;
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
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct PerFrameData
{
    vec4 u;
    vec4 v;
    vec4 w;
    vec4 cameraPosition;
    uint frameNumber;
    uint totalFrameNumber;
    uint lightNumber;
    uint renderMode;
    float4x4 envMapTransform;
    float4x4 previousProjView;
};

struct LightParameter
{
    vec4 position;
    vec4 u;
    vec4 v;
    vec4 emission;
    vec4 normalAndPdf;
};

struct WaveletShaderParameters
{
    int level;
};