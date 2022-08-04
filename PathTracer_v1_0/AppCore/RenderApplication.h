#pragma once
#include "Framework.h"
#include "Scene.h"
#include "RenderTexture.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <fstream>
#include "SVGF/SVGFPass.h"
#include "ReSTIR/ReSTIR.h"
#include "Pathtracer/PathTracer.h"

#include "Tonemap/ToneMapper.h"
#include "HeapData.h"
#include "SceneResourceManager.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

static const int FRAME_ACCUMULATE_NUMBER = 10;

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
    //RenderContext* mpRenderContext;

    static const uint32_t kRtvHeapSize = 50;

    // uint64_t mTlasSize = 0;

    Scene *scene;

    void createRtPipelineState();
    //ID3D12StateObjectPtr mpPipelineState;
    //ID3D12RootSignaturePtr mpEmptyRootSig;

    void createShaderTable();
    //ID3D12ResourcePtr mpShaderTable;
    uint32_t mShaderTableEntrySize = 0;

    void createShaderResources();
   // std::map<string, ID3D12ResourcePtr> outputUAVBuffers;

    //ID3D12ResourcePtr mpCameraConstantBuffer = nullptr;
    //ID3D12ResourcePtr mpLightParametersBuffer = nullptr;
    //ID3D12ResourcePtr mpPrevReservoirBuffer = nullptr;

    //std::vector<ID3D12ResourcePtr> mpTextureBuffers;
    //D3D12_GPU_DESCRIPTOR_HANDLE mpTextureStartHandle;

    // int textureStartHeapOffset;

    IDirectInput8A* mpInput = 0;
    IDirectInputDevice8A* mpKeyboard = 0;
    unsigned char mpKeyboardState[256];
    unsigned char mpKeyboardStatePrev[256];

    //D3D12_CPU_DESCRIPTOR_HANDLE g_SceneMeshInfo;

    uint32_t mFrameNumber = 1;
    uint32_t mTotalFrameNumber = 0;
    std::ofstream logFile;
    std::chrono::steady_clock::time_point lastTime;

    float elapsedTime[FRAME_ACCUMULATE_NUMBER];
    
    uint renderMode = 0;
    bool doPostProcess = false;
    

    // Post processing
    PostProcessQuad* postProcessQuad;
    SVGFPass* svgfPass;
    ToneMapper* tonemapPass;
    ReSTIR* restirPass;
    PathTracer* pathTracer;
    bool mDirty = false;

    Shader* defaultCopyShader;
    ID3D12ResourcePtr mParamBuffer = nullptr;

    ID3D12DescriptorHeapPtr g_pd3dSrvDescHeap;

    bool show_demo_window = false;
    bool show_another_window = false;
};