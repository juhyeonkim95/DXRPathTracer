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
    mat4x4 envMapTransform;
    mat4x4 previousProjView;
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
    vec4 position;
    vec4 u;
    vec4 v;
    vec4 emission;
    vec4 normalAndPdf;
};

struct LightSample
{
    vec3 position;
    float pdf;
    vec3 normal;
    uint lightIndex;
    vec3 Li;
    float unused;
};

struct Reservoir
{
    float wSum;
    float W;
    float M;
    float unused;
    LightSample lightSample;
};