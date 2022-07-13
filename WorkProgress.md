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
- Try to apply texture. Creating buffer is done, but uploading texture data is troublesome.
- Exploit external library (DirectXTex)

## 20220708
- Load a single texture using DirectXTex.
- Implement per-instance texture loading using per-instance SRV.
- Working on loading XML file better way. 

## 20220711
- Implement new geometry (rectangle, cube) by transforming it into a mesh. (no additional intersection program)
- Implement a simple rectangular emitter.
- Diffuse only real-time path tracer.

## 20220712
- Implement Multiple Importance Sampling with single light

## 20220713
- Reduce recursion depth by moving shadowray program from closest hit to raygen program.
| Frame   | depth 2 | depth 1  |
|---------|---------|----------|
| 1       | 1389328 | 953796   |
| 2       | 1375597 | 959725   |
| 3       | 1354166 | 978917   |
| 4       | 1350501 | 940901   |
| 5       | 1351036 | 944522   |
| 6       | 1366442 | 941524   |
| Average | 1364512 | 953230.8 |
