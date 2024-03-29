# DXRPathTracer

This is a real-time 1spp path tracer based on DirectX 12 RayTracing.
Detailed daily work process could be found [here](WorkProgress.md).

![Teaser](assets/20220816_teaser_v1.jpg)

Followings are real-time rendered images.
It works stable at 60 FPS (RTX 3060).

![Teaser](assets/20220818_teaser_v4.gif)
![Teaser](assets/20220818_teaser_v5.gif)

Additional details could be found here.
- [Material info](MaterialInfo.md)
- [Rendering pipeline](RenderPipeline.md)
- [Renderpass details](RenderPassDetails.md)

## Project Overview
The goal of this project is to implement a scalable path tracer that runs in real-time.
Implementing path tracer largely refered from [Mitsuba renderer](https://github.com/mitsuba-renderer/mitsuba)
and my previous custom [path tracer](https://github.com/juhyeonkim95/MitsubaPyOptiX).
In order to achieve real-timeness, I try two approahces.
First is to exploit denoising techniques.
Second is to employ better sampling strategies.
Among the various methods, I choose to implement 
[SVGF](https://cg.ivd.kit.edu/publications/2017/svgf/svgf_preprint.pdf) &
[ReLAX](https://www.nvidia.com/en-us/on-demand/session/gtcspring21-s32759/) and 
[ReSTIR](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct) respectively.

## Project Timeline
- Week 1~3 : Implement basic path tracer using DirectX12.
- Week 4,5 : Denoising based on [SVGF](https://cg.ivd.kit.edu/publications/2017/svgf/svgf_preprint.pdf).
- Week 6 : Enhance sampling quality using [ReSTIR](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct)
- Week 7,8 : Denoising revisited. Implement advanced algorithm [ReLAX](https://www.nvidia.com/en-us/on-demand/session/gtcspring21-s32759/).

## Requirements
This project is implemented based on the framework of [DxrTutorials](https://github.com/NVIDIAGameWorks/DxrTutorials).

It was developed using Visual Studio 2019 16.11. 

Windows 10 SDK 19041 or later is required.

You also need to download and import [DirectXTex](https://github.com/microsoft/DirectXTex).

## Scene Data
For scene data format, [Mitsuba renderer](https://github.com/mitsuba-renderer/mitsuba)'s format was used.
Refer mitsuba renderer for the details.
Example scenes could be downloaded from [here](https://benedikt-bitterli.me/resources/).
Since scene loading was not the main focus of the project, its implementation is at minimal level.
It can load and render only limited scenes.
I have tested with Country Kitchen, The Wooden Staircase, The White Room and Salle de bain.
Scene with environment map is not fully implemented.

## Implementation Progress
### (Week 1~3) Core Path Tracer
#### Shape
- [x] Mesh
- [x] Rectangle
- [x] Cube
- [ ] Sphere
- [ ] Disk
- [ ] Cylinder
#### BSDF
- [x] Diffuse
- [x] Conductor
- [x] Rough Conductor
- [x] Dielectric
- [x] Rough Dielectric
- [x] Plastic
- [x] Rough Plastic
- [ ] Disney

For details, refer [this document](MaterialInfo.md).

#### Emitter
- [x] Area (rectangle only)
- [x] Environment (only without NEE/MIS)
- [ ] Point
- [ ] Directional
- [ ] Spotlight

#### Integrator (Rendering Equation Estimator)
- [x] Path Tracer
- [x] Path Tracer with NEE/MIS (direct light sampling)
- [ ] Guided Path Tracer

### (Week 4, 5) Denoising (SVGF)
- [x] Temporal accumulation
- [x] Disocclusion fix
- [x] A-Trous wavelet filter

### (Week 6) Enhancing Sampling Quality (ReSTIR)
- [x] Basic Resampling
- [x] Temporal Reuse
- [ ] Spatial Reuse

### (Week 7, 8) Render Pass & Advanced denoising
- [x] Falcor-styled render pass structuring (Antialiasing, Tonemap, etc..)
- [x] Delta reflection motion vector
- [x] Delta transmission motion vector
- [x] RELAX diffuse version
- [x] RELAX specular version

Because of the time limit, RELAX implementation is quite different from the original one.
I only managed to develop motion vectors for delta reflection / tramission and split the renderpass, and still used a large portion of SVGF code.
If you want to know the exact implementation of RELAX, please refer to the [original code](https://github.com/NVIDIAGameWorks/RayTracingDenoiser).

## Not Implemented
 - Efficient & scalable scene loader
 - Exact RELAX & delta motion vector implementation
 - ReSTIR with spatial reuse

## Acknowledgement
This project was supported by [PearlAbyss](https://www.pearlabyss.com/ko-kr).
