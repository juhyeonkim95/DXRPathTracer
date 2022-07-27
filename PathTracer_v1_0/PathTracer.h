#pragma once
#include "Framework.h"
#include "Scene.h"
#include "RenderTexture.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <fstream>
#include "Shader.h"


#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

static const int FRAME_ACCUMULATE_NUMBER = 10;

class TutorialPathTracer : public Tutorial
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
    struct HeapData
    {
        ID3D12DescriptorHeapPtr pHeap;
        uint32_t usedEntries = 0;
    };
    HeapData mRtvHeap;
    static const uint32_t kRtvHeapSize = 30;

    void createAccelerationStructures();
    AccelerationStructureBuffers createTopLevelAccelerationStructure();
    ID3D12ResourcePtr mpVertexBuffer;
    ID3D12ResourcePtr mpTopLevelAS;
    std::vector<ID3D12ResourcePtr> mpBottomLevelAS;

    uint64_t mTlasSize = 0;

    Scene *scene;

    void createRtPipelineState();
    ID3D12StateObjectPtr mpPipelineState;
    ID3D12RootSignaturePtr mpEmptyRootSig;

    void createShaderTable();
    ID3D12ResourcePtr mpShaderTable;
    uint32_t mShaderTableEntrySize = 0;

    void createShaderResources();
    void createTextureShaderResources();

    std::map<string, ID3D12ResourcePtr> outputUAVBuffers;
    std::map<string, int> mSrvHeapIndexMap;


    ID3D12DescriptorHeapPtr mpSrvUavHeap;
    static const uint32_t kSrvUavHeapSize = 2;
    int mpSrvUavHeapCount = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandler(int index);
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandlerByName(const char* name);


    //void createConstantBuffer();
    ID3D12ResourcePtr mpCameraConstantBuffer = nullptr;
    ID3D12ResourcePtr mpMaterialBuffer = nullptr;
    ID3D12ResourcePtr mpGeometryInfoBuffer = nullptr;
    ID3D12ResourcePtr mpLightParametersBuffer = nullptr;
    ID3D12ResourcePtr mpWaveletParameterBuffer = nullptr;


    ID3D12ResourcePtr mpIndicesBuffer = nullptr;
    ID3D12ResourcePtr mpVerticesBuffer = nullptr;
    std::vector<ID3D12ResourcePtr> mpTextureBuffers;
    int textureStartHeapOffset;

    //std::vector<ID3D12ResourcePtr> mpMaterialConstantBuffer;

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
    
    uint renderMode;
    bool doPostProcess = true;

    void createUAVBuffer(DXGI_FORMAT format, std::string name, uint depth = 1);
    void createSRVTexture(DXGI_FORMAT format, std::string name);

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;

    RenderTexture *createRenderTexture(DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    void createRenderTextures();
    

    RenderTexture *motionVectorRenderTexture;
    RenderTexture *historyLengthRenderTexture;

    Shader* depthDerivativeShader;
    RenderTexture *depthDerivativeRenderTexture;

    Shader* motionVectorShader;

    RenderTexture* temporalAccumulationTextureDirect;
    RenderTexture* temporalAccumulationTextureDirectMoment;

    RenderTexture* temporalAccumulationTextureIndirect;
    RenderTexture* temporalAccumulationTextureIndirectMoment;


    Shader* temporalAccumulationShader;

    Shader* reconstructionShader;
    RenderTexture* reconstructionRenderTexture;

    vector<RenderTexture*> waveletDirect;
    vector<RenderTexture*> waveletIndirect;
    int waveletCount = 2;
    Shader* waveletShader;


    Shader* defaultCopyShader;
    void copyRenderTexture(RenderTexture* dest, RenderTexture* source);

    Shader* tonemapShader;

    // RenderTexture renderTexture;

    // PostProcessing
    // ID3D12RootSignature* postProcessRootSignature;
    D3D12_VIEWPORT viewport; // area that output from rasterizer will be stretched to.
    D3D12_RECT scissorRect; // the area to draw in. pixels outside that area will not be drawn onto
    // ID3D12PipelineState* pipelineStateObject; // pso containing a pipeline state
    ID3D12Resource* vertexBuffer; // a default buffer in GPU memory that we will load vertex data for our triangle into
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // a structure containing a pointer to the vertex data in gpu memory
                                               // the total size of the buffer, and the size of each element (vertex)

};