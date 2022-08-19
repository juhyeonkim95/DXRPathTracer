#ifndef PATHTRACERNRD_DESCRIPTOR_TABLE_HLSLI
#define PATHTRACERNRD_DESCRIPTOR_TABLE_HLSLI

RWTexture2D<float4> gDirectIllumination : register(u2);
RWTexture2D<float4> gIndirectIllumination : register(u3);

RWTexture2D<float4> gDiffuseRadiance : register(u4);
RWTexture2D<float4> gSpecularRadiance : register(u5);
RWTexture2D<float4> gEmission : register(u6);

RWTexture2D<float4> gReflectance : register(u7);
RWTexture2D<float4> gDiffuseReflectance : register(u8);
RWTexture2D<float4> gSpecularReflectance : register(u9);

RWTexture2D<float4> gDeltaReflectionReflectance : register(u10);
RWTexture2D<float4> gDeltaReflectionEmission : register(u11);
RWTexture2D<float4> gDeltaReflectionRadiance : register(u12);

RWTexture2D<float4> gDeltaTransmissionReflectance : register(u13);
RWTexture2D<float4> gDeltaTransmissionEmission : register(u14);
RWTexture2D<float4> gDeltaTransmissionRadiance : register(u15);


RWTexture2D<float4> gResidualRadiance : register(u16);
RWTexture2D<uint> gPrimaryPathType : register(u17);
RWTexture2D<float> gRoughness : register(u18);
RWTexture2D<float2> gMotionVector : register(u19);


RWTexture2D<float4> gPositionMeshID : register(u20);
RWTexture2D<float4> gPositionMeshIDPrev : register(u21);

RWTexture2D<float4> gNormalDepth : register(u22);
RWTexture2D<float4> gNormalDepthPrev : register(u23);

RWTexture2D<float4> gDeltaReflectionPositionMeshID : register(u24);
RWTexture2D<float4> gDeltaReflectionNormal : register(u25);
RWTexture2D<float4> gDeltaTransmissionPositionMeshID : register(u26);
RWTexture2D<float4> gDeltaTransmissionNormal : register(u27);

#endif