# DXRPathTracer
This is a real-time path tracer based on DirectX RayTracing.
Daily work process could be found [here](WorkProgress.md).

## Requirements
This project is implemented based on the framework of [DxrTutorials](https://github.com/NVIDIAGameWorks/DxrTutorials).

It was developed using Visual Studio 2019 16.11. 

Windows 10 SDK 19041 or later is required.

You also need to install [DirectXTex](https://github.com/microsoft/DirectXTex).

## Scene Data
For scene data format, [Mitsuba renderer](https://github.com/mitsuba-renderer/mitsuba)'s format was used.
Refer mitsuba renderer for the details.
Example scenes could be downloaded from [here](https://benedikt-bitterli.me/resources/).

## Example Scene 
![20220713_mis_cornellbox](https://user-images.githubusercontent.com/59192387/178707155-974f0043-6cdd-4e84-8580-280826ca9310.PNG)

## Implementation Progress
### Shape
- [x] Mesh
- [x] Rectangle
- [x] Cube
- [ ] Sphere
- [ ] Disk
- [ ] Cylinder

### BSDF
- [x] Diffuse
- [x] Conductor
- [x] Rough Conductor
- [x] Dielectric
- [x] Rough Dielectric
- [x] Plastic
- [ ] Rough Plastic
- [ ] Disney

### Emitter
- [x] Area (rectangle only)
- [x] Environment
- [ ] Point
- [ ] Directional
- [ ] Spotlight


### Integrator (Rendering Equation Estimator)
- [x] Path Tracer
- [x] Path Tracer with MIS (direct light sampling)
- [ ] Path Tracer with MIS / multiple lights
- [ ] Guided Path Tracer

## Usage
(WIP)
