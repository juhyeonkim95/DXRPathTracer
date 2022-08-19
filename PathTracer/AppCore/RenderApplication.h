#pragma once
#include "Framework.h"
#include "Scene.h"
#include "RenderTexture.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <fstream>
#include "SVGF/SVGFPass.h"
#include "RELAX/RELAXPass.h"
#include "RELAXSingle/RELAXSinglePass.h"
#include "Timer.h"
#include "ModulateIllumination/ModulateIllumination.h"
#include "ReSTIR/ReSTIR.h"
#include "PathTracer/PathTracer.h"
#include "BlendPass/BlendPass.h"
#include "Tonemap/ToneMapper.h"
#include "AntiAliasing/FXAA/FXAA.h"
#include "AntiAliasing/TAA/TAA.h"
#include "HeapData.h"
#include "SceneResourceManager.h"
#include "MotionVectorDeltaReflection/MotionVectorDeltaReflection.h"
#include "MotionVectorDeltaTransmission/MotionVectorDeltaTransmission.h"
#include "DepthDerivative/DepthDerivativePass.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

static const uint32_t kDefaultSwapChainBuffers = 2;  // TODO : change?

class RenderApplication : public Application
{
public:
    RenderApplication(bool useVsync);

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
    void renderGUI(uint32_t rtvIndex);

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


    // keyboard inputs
    IDirectInput8A* mpInput = 0;
    IDirectInputDevice8A* mpKeyboard = 0;
    unsigned char mpKeyboardState[256];
    unsigned char mpKeyboardStatePrev[256];

    uint32_t mFrameNumber = 1;
    uint32_t mTotalFrameNumber = 0;
    std::ofstream mLogFile;
    std::chrono::steady_clock::time_point mLastFrameTimeStamp;

    Timer* initializationTimer;
    Timer* perFrameTimer;

    // Just for faster debug
    // 0 --> denoised output
    // 1~9 --> other output whatever you want.
    uint renderMode = 0;
    
    // Render Pass
    PostProcessQuad* postProcessQuad;
    PathTracer* pathTracer;
    ReSTIR* restirSetting;
    
    DepthDerivativePass* depthDerivativePass;

    // Denoising
    RELAXPass* diffuseFilterPass;
    RELAXPass* specularFilterPass;
    RELAXPass* deltaReflectionFilterPass;
    RELAXPass* deltaTransmissionFilterPass;
    RELAXPass* residualFilterPass;

    // processing diffuse/specular/delta reflection/transmission at once slightly improves the performance.
    RELAXSinglePass* allInOneFilterPass;

    bool processAllInOne = true;
    bool mUseVSync = true;

    // Motion vector renderpass
    MotionVectorDeltaReflection* deltaReflectionMotionVectorPass;
    MotionVectorDeltaTransmission* deltaTransmissionMotionVectorPass;

    ModulateIllumination* modulatePass;

    BlendPass* blendPass;
    FXAA* fxaaPass;
    TAA* taaPass;

    ToneMapper* tonemapPass;
    bool mDirty = true;


    ID3D12DescriptorHeapPtr mImguiSrvDescHeap;
};