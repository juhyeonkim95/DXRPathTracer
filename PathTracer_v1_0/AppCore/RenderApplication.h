#pragma once
#include "Framework.h"
#include "Scene.h"
#include "RenderTexture.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <fstream>
#include "SVGF/SVGFPass.h"
#include "RELAX/RELAXPass.h"
#include "RELAXSpecular/RELAXSpecularPass.h"
#include "ModulateIllumination/ModulateIllumination.h"
#include "ReSTIR/ReSTIR.h"
#include "PathTracer/PathTracer.h"
#include "BlendPass/BlendPass.h"
#include "Tonemap/ToneMapper.h"
#include "AntiAliasing/FXAA/FXAA.h"
#include "AntiAliasing/TAA/TAA.h"
#include "HeapData.h"
#include "SceneResourceManager.h"
#include "MotionVector/MotionVector.h"
#include "MotionVectorSpecular/MotionVectorSpecular.h"
#include "MotionVectorDeltaReflection/MotionVectorDeltaReflection.h"
#include "MotionVectorDeltaTransmission/MotionVectorDeltaTransmission.h"
#include "DepthDerivative/DepthDerivativePass.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

static const int FRAME_ACCUMULATE_NUMBER = 32;

class RenderApplication : public Application
{
public:
    Scene* getScene() { return this->scene; }
    void setScene(Scene* scene) { this->scene = scene; }

    void onLoad(HWND winHandle, uint32_t winWidth, uint32_t winHeight) override;
    void onFrameRender() override;
    void onShutdown() override;
    void update();
    void initPostProcess();
    void postProcess(int rtvIndex);

private:
    void initDX12(HWND winHandle, uint32_t winWidth, uint32_t winHeight);
    uint32_t beginFrame();
    void endFrame(uint32_t rtvIndex);
    HWND mHwnd = nullptr;
    ID3D12Device5Ptr mpDevice;
    ID3D12CommandQueuePtr mpCmdQueue;
    IDXGISwapChain3Ptr mpSwapChain;
    uvec2 mSwapChainSize;
    ID3D12GraphicsCommandList4Ptr mpCmdList;
    ID3D12FencePtr mpFence;
    HANDLE mFenceEvent;
    uint64_t mFenceValue = 0;

    struct
    {
        ID3D12CommandAllocatorPtr pCmdAllocator;
        ID3D12ResourcePtr pSwapChainBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    } mFrameObjects[kDefaultSwapChainBuffers];


    // Heap data
    HeapData *mRtvHeap;
    HeapData* mpSrvUavHeap;
    SceneResourceManager* sceneResourceManager;

    static const uint32_t kRtvHeapSize = 100;
    static const uint32_t kSrvUavHeapSize = 200;

    Scene *scene;

    IDirectInput8A* mpInput = 0;
    IDirectInputDevice8A* mpKeyboard = 0;
    unsigned char mpKeyboardState[256];
    unsigned char mpKeyboardStatePrev[256];

    uint32_t mFrameNumber = 1;
    uint32_t mTotalFrameNumber = 0;
    std::ofstream logFile;
    std::chrono::steady_clock::time_point lastTime;

    float elapsedTime[FRAME_ACCUMULATE_NUMBER];
    
    uint renderMode = 0;
    bool doPostProcess = false;
    

    // Post processing
    PostProcessQuad* postProcessQuad;
    PathTracer* pathTracer;
    ReSTIR* restirPass;
    SVGFPass* svgfPass;
    
    DepthDerivativePass* depthDerivativePass;

    RELAXPass* diffuseFilterPass;
    RELAXSpecularPass* specularFilterPass;
    RELAXPass* deltaReflectionFilterPass;
    RELAXPass* deltaTransmissionFilterPass;
    RELAXPass* residualFilterPass;

    MotionVectorDeltaReflection* deltaReflectionMotionVectorPass;
    MotionVectorDeltaTransmission* deltaTransmissionMotionVectorPass;
    MotionVector* motionVectorPass;
    MotionVectorSpecular* motionVectorSpecularPass;

    ModulateIllumination* modulatePass;


    bool createSVGF = false;

    BlendPass* blendPass;
    FXAA* fxaaPass;
    TAA* taaPass;

    ToneMapper* tonemapPass;
    bool mDirty = true;

    Shader* defaultCopyShader;
    ID3D12ResourcePtr mParamBuffer = nullptr;

    ID3D12DescriptorHeapPtr g_pd3dSrvDescHeap;
};