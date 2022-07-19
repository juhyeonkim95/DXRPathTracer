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
- Implement MIS(Multiple Importance Sampling) with single light
- Code refactoring

## 20220713
- Reduce recursion depth by moving shadowray program from closest hit to raygen program.

Performance Comparison for kitchen scene (256 spp, ms)

| Frame   | depth 2 | depth 1 |
|---------|---------|---------|
| 1       | 1389    | 953     |
| 2       | 1375    | 959     |
| 3       | 1354    | 978     |
| 4       | 1350    | 940     |
| 5       | 1351    | 944     |
| 6       | 1366    | 941     |
| Average | 1364    | 953     |

About x1.5 speed up!

- Working on organizing BSDF sampling / pdf / eval and light sampling code.

## 20220714
- Implement an environment light.
- Implement Conductor
- Implement Dielectric
![image](https://user-images.githubusercontent.com/59192387/178934324-231aa08f-beec-44b8-81ae-d4d909c4b594.png)

## 20220715
- Implement Rough Conductor
- Implement Rough Dielectric
- Implement Plastic
![image](https://user-images.githubusercontent.com/59192387/179181050-dbb5f661-0b77-49e4-b3f7-304a25e3abfa.png)

## 20220718
- Try to apply filtering & denoising.
- Reading SVGF, A-SVGF paper.

## 20220719
- Try to implement SVGF
- MIS with multiple light (uniformly select one light)
