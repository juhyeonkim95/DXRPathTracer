#ifndef DESCRIPTORTABLE_HLSLI
#define DESCRIPTORTABLE_HLSLI

#include "../Common/CommonStructs.hlsli"

RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gOutputHDR : register(u1);


RaytracingAccelerationStructure gRtScene : register(t0);
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

cbuffer ApplicationParams : register(b2)
{
    PathTracerParams gPathTracer;
    ReSTIRParams gReSTIR;
}

#endif