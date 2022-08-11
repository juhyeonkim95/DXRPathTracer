#ifndef DESCRIPTORTABLE_HLSLI
#define DESCRIPTORTABLE_HLSLI

#include "../Common/CommonStructs.hlsli"

RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gOutputHDR : register(u1);
RWTexture2D<float4> gDirectIllumination : register(u2);
RWTexture2D<float4> gIndirectIllumination : register(u3);
RWTexture2D<float4> gDiffuseRadiance : register(u4);
RWTexture2D<float4> gSpecularRadiance : register(u5);
RWTexture2D<float4> gEmission : register(u6);

RWTexture2D<float4> gReflectance : register(u7);
RWTexture2D<float4> gDiffuseReflectance : register(u8);
RWTexture2D<float4> gSpecularReflectance : register(u9);

RWTexture2D<float4> gOutputPositionGeomID : register(u10);
RWTexture2D<float4> gOutputNormal : register(u11);
RWTexture2D<float4> gOutputPositionGeomIDPrev : register(u12);
RWTexture2D<float4> gOutputNormalPrev : register(u13);


RWTexture2D<float4> gDeltaReflectionReflectance : register(u16);
RWTexture2D<float4> gDeltaReflectionEmission : register(u17);
RWTexture2D<float4> gDeltaReflectionRadiance : register(u18);

RWTexture2D<float4> gDeltaTransmissionReflectance : register(u19);
RWTexture2D<float4> gDeltaTransmissionEmission : register(u20);
RWTexture2D<float4> gDeltaTransmissionRadiance : register(u21);


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