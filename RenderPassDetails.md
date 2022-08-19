# Render Pass Details
This document explains different renderpass in `RenderPass` folder.
The overall structure is very similar to that of Falcor.

## Path Tracer
Path Tracer is a main render pass.
It not only outputs radiance, but also several additional values such as position, reflected position, which will be used in denoising pass.
Note that path tracer pass also does a role of G-Buffer.

Here is a list of used UAVs for path tracer. They are referred by a single descriptor.

| Index | Name                                 | Format              | Description                                                                                                                                         |
| ----- | ------------------------------------ | ------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0     | gOutput                              | R8G8B8A8\_UNORM     | LDR output                                                                                                                                          |
| 1     | gOutputHDR                           | R32G32B32A32\_FLOAT | HDR output                                                                                                                                          |
| 2     | gDirectIllumination                  | R32G32B32A32\_FLOAT | Direct illumination (used for SVGF, deprecated)                                                                                                     |
| 3     | gIndirectIllumination                | R32G32B32A32\_FLOAT | Indirect illumination (used for SVGF, deprecated)                                                                                                   |
| 4     | gDiffuseRadiance                     | R32G32B32A32\_FLOAT | Radiance of the path which is classified as diffuse (demodulated)                                                                                   |
| 5     | gSpecularRadiance                    | R32G32B32A32\_FLOAT | Radiance of the path which is classified as specular (demodulate)                                                                                   |
| 6     | gEmission                            | R32G32B32A32\_FLOAT | Emission                                                                                                                                            |
| 7     | gReflectance                         | R8G8B8A8\_UNORM     | Reflectance of primary ray (used for SVGF, deprecated)                                                                                              |
| 8     | gDiffuseReflectance                  | R8G8B8A8\_UNORM     | Diffuse reflectance of primary ray                                                                                                                  |
| 9     | gSpecularReflectance                 | R8G8B8A8\_UNORM     | Specular reflectance of primary ray                                                                                                                 |
| 10    | gDeltaReflectionReflectance          | R8G8B8A8\_UNORM     | Reflectance of first hit point after reflection                                                                                                     |
| 11    | gDeltaReflectionEmission             | R32G32B32A32\_FLOAT | Emission of first hit point after reflection                                                                                                        |
| 12    | gDeltaReflectionRadiance             | R32G32B32A32\_FLOAT | Radiance of the path which is classified as delta reflection (demodulate)                                                                           |
| 13    | gDeltaTransmissionReflectance        | R8G8B8A8\_UNORM     | Reflectance of first hit point after series of transmission                                                                                         |
| 14    | gDeltaTransmissionEmission           | R32G32B32A32\_FLOAT | Emission of first hit point after series of transmission                                                                                            |
| 15    | gDeltaTransmissionRadiance           | R32G32B32A32\_FLOAT | Radiance of the path that only experiences delta transmission (TIR is exception) before first non-delta hit (demodulate)                            |
| 16    | gResidualRadiance                    | R32G32B32A32\_FLOAT | Radiance of the path that first experiences delta transmission and mix of delta reflection/transmission before first non-delta hit (not demodulate) |
| 17    | gPathType                            | R32\_UINT           | Reflection lobe type of primary ray                                                                                                                 |
| 18    | gRoughness                           | R32\_FLOAT          | Roughness of primary ray                                                                                                                            |
| 19    | gMotionVector                        | R32G32\_FLOAT       | Previous frame pixel value                                                                                                                          |

From now, since they will be swapped with previous buffer, so will be referred by a different descriptor respectively.
30~33 is not visible at the path tracer shader.

| Index | Name                                 | Format              | Description                                                                                                                                         |
| ----- | ------------------------------------ | ------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| 20    | gPositionMeshID                      | R32G32B32A32\_FLOAT | Primary ray                                                                                                                                         |
| 21    | gPositionMeshIDPrev                  | R32G32B32A32\_FLOAT | Primary ray                                                                                                                                         |
| 22    | gNormalDepth                         | R32G32B32A32\_FLOAT | Primary ray                                                                                                                                         |
| 23    | gNormalDepthPrev                     | R32G32B32A32\_FLOAT | Primary ray                                                                                                                                         |
| 24    | gDeltaReflectionPositionMeshID       | R32G32B32A32\_FLOAT | First hit point after delta reflection                                                                                                              |
| 25    | gDeltaReflectionNormal               | R8G8B8A8\_SNORM     | First hit point after delta reflection                                                                                                              |
| 26    | gDeltaTransmissionPositionMeshID     | R32G32B32A32\_FLOAT | First non-delta hit point after series of delta transmission                                                                                        |
| 27    | gDeltaTransmissionNormal             | R8G8B8A8\_SNORM     | First non-delta hit point after series of delta transmission                                                                                        |
| 28    | gPrevReserviors                      |  -                  | Used for ReSTIR                                                                                                                                     |
| 29    | gCurrReserviors                      |  -                  | Used for ReSTIR                                                                                                                                     |
| 30    | gDeltaReflectionPositionMeshIDPrev   | R32G32B32A32\_FLOAT | First hit point after delta reflection                                                                                                              |
| 31    | gDeltaTransmissionPositionMeshIDPrev | R32G32B32A32\_FLOAT | First hit point after delta reflection                                                                                                              |
| 32    | gDeltaReflectionNormalPrev           | R8G8B8A8\_SNORM     | First non-delta hit point after series of delta transmission                                                                                        |
| 33    | gDeltaTransmissionNormalPrev         | R8G8B8A8\_SNORM     | First non-delta hit point after series of delta transmission                                                                                        |

From now, we explain additional passes.
They are all implemented based on pixel shader.
Details of input/output will be omitted and briefly explains the role of each pass only.

## Depth Derivative
This pass calculated depth derivative using `ddx, ddy` in HLSL.

## MotionVectorDeltaReflection / DeltaTransmission
This pass calculates motion vector as explained in [this document](RenderPipeline.md).

##  RELAX

### Temporal Accululation
This shader performs temporal accumulation with consistency check.

### Disocclusion Fix
This shader performs disocclusion fix.

### A-Trous Wavelet Filter
This shader performs A-Trous wavelet filtering.


## RELAXSingle
RELAX is merged version of RELAX which can handle diffuse, specular, delta reflection, delta transmission at once.
This slightly improves the performance, so recommended to be used.
Other thing is just copy and paste of RELAX.

##  SVGF
This is deprecated and will not work.
Use RELAX version instead.

## BlendPass
This pass blends output with GT path tracer output.
Supports two mode
 - Temporal blend : blend more GT path tracer output as time goes.
 - Divide mode : divide the screen that show both outputs at once (for debug)

## AntiAliasing
Currently FXAA, TAA is implemented, but TAA doesn't work correctly.

## ModulateIllumination
This pass integrates illumination as explained in [this document](RenderPipeline.md).

## Tonemap
This pass performs tonemapping & sRGB convert.

## ReSTIR
This is not a render pass.
It is included as a part of main path tracer pass.