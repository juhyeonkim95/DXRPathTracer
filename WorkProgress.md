This document describes daily work progess of the project.

## 20220630, 20220701
Understand basics of DXR (reference : https://intro-to-dxr.cwyman.org/)
- Raytracing pipeline (anyhit-closesthit-miss)
- DXR Initialization & setup
- Acceleration structure (BLAS, TLAS)
- Root signature & association
- Ray tracing pipeline state object (RTPSO)
- Shader table


## 20220704
- Implemented a very simple ray tracer that loads meshes and show geometry only.
![image](https://user-images.githubusercontent.com/59192387/177284063-e5416ec8-654d-4675-afe5-ab1a283b0973.png)

## 20220705
- Implement very simple ConstantBuffer that includes mesh color
- Need to change StructuredBuffer?

## 20220706
- Still studying StructuredBuffer
- In order to access normal, uv information, implementing StructuredBuffer is inevitable!
- Changed mesh color buffer from ConstantBuffer to StructuredBuffer.
- How to implement normal/uv information buffer? (StructuredBuffer vs ByteAddressBuffer)

## 20220707
- Load UV/Normal infomation using StructuredBuffer
