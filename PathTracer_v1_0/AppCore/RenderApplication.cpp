#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include "RenderApplication.h"
#include "DX12Utils.h"
#include "DX12BufferUtils.h"
#include "CommonStruct.h"
#include "d3dx12.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "BSDF/BSDFLobes.h"


void RenderApplication::initDX12(HWND winHandle, uint32_t winWidth, uint32_t winHeight)
{
    mHwnd = winHandle;
    mSwapChainSize = uvec2(winWidth, winHeight);

    // Initialize the debug layer for debug builds
#ifdef _DEBUG
    ID3D12DebugPtr pDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
    }
#endif
    // Create the DXGI factory
    IDXGIFactory4Ptr pDxgiFactory;
    d3d_call(CreateDXGIFactory1(IID_PPV_ARGS(&pDxgiFactory)));
    mpDevice = createDevice(pDxgiFactory);
    mpCmdQueue = createCommandQueue(mpDevice);
    mpSwapChain = createDxgiSwapChain(pDxgiFactory, mHwnd, winWidth, winHeight, DXGI_FORMAT_R8G8B8A8_UNORM, mpCmdQueue);

    // Create a RTV descriptor heap
    mRtvHeap = new HeapData(mpDevice, kRtvHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    // Create the per-frame objects
    for (uint32_t i = 0; i < kDefaultSwapChainBuffers; i++)
    {
        d3d_call(mpDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mFrameObjects[i].pCmdAllocator)));
        d3d_call(mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&mFrameObjects[i].pSwapChainBuffer)));

        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Texture2D.MipSlice = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvHeap->addDescriptorHandle();

        mpDevice->CreateRenderTargetView(mFrameObjects[i].pSwapChainBuffer, &desc, rtvHandle);

        mFrameObjects[i].rtvHandle = rtvHandle;
    }

    // Create the command-list
    d3d_call(mpDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrameObjects[0].pCmdAllocator, nullptr, IID_PPV_ARGS(&mpCmdList)));

    // Create a fence and the event
    d3d_call(mpDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mpFence)));
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

uint32_t RenderApplication::beginFrame()
{
    // Bind the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { mpSrvUavHeap->getDescriptorHeap() };
    mpCmdList->SetDescriptorHeaps(arraysize(heaps), heaps);
    return mpSwapChain->GetCurrentBackBufferIndex();
}

void RenderApplication::endFrame(uint32_t rtvIndex)
{
    resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
    mpSwapChain->Present(0, 0);

    // Prepare the command list for the next frame
    uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();

    // Make sure we have the new back-buffer is ready
    if (mFenceValue > kDefaultSwapChainBuffers)
    {
        mpFence->SetEventOnCompletion(mFenceValue - kDefaultSwapChainBuffers + 1, mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
    }

    mFrameObjects[bufferIndex].pCmdAllocator->Reset();
    mpCmdList->Reset(mFrameObjects[bufferIndex].pCmdAllocator, nullptr);
}


//////////////////////////////////////////////////////////////////////////
// Callbacks
//////////////////////////////////////////////////////////////////////////
void RenderApplication::onLoad(HWND winHandle, uint32_t winWidth, uint32_t winHeight)
{

    initDX12(winHandle, winWidth, winHeight);
    
    // init input
    DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)(&mpInput), nullptr);
    mpInput->CreateDevice(GUID_SysKeyboard, &mpKeyboard, nullptr);
    mpKeyboard->SetDataFormat(&c_dfDIKeyboard);
    mpKeyboard->Acquire();

    // Create an SRV/UAV descriptor heap
    mpSrvUavHeap = new HeapData(mpDevice, kSrvUavHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // load scene resources
    sceneResourceManager = new SceneResourceManager(scene, mpDevice, mpSrvUavHeap);
    sceneResourceManager->createSceneAccelerationStructure(mpCmdQueue, mFrameObjects[0].pCmdAllocator, mpCmdList, mpFence, mFenceEvent, mFenceValue);
    sceneResourceManager->createSceneSRVs();
    sceneResourceManager->createSceneTextureResources(mpCmdQueue, mFrameObjects[0].pCmdAllocator, mpCmdList, mpFence, mFenceEvent, mFenceValue);

    // add path tracer shader// init render passes
    pathTracer = new PathTracer(mpDevice, scene, mSwapChainSize);
    pathTracer->createShaderResources(mpSrvUavHeap);
    pathTracer->createShaderTable(mpSrvUavHeap);

    restirPass = new ReSTIR(mpDevice);

    //svgfPass = new SVGFPass(mpDevice, mSwapChainSize);
    //svgfPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    depthDerivativePass = new DepthDerivativePass(mpDevice, mSwapChainSize);
    depthDerivativePass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    diffuseFilterPass = new RELAXPass(mpDevice, mSwapChainSize, RELAX_TYPE::RELAX_DIFFUSE, BSDF_LOBE::BSDF_LOBE_DIFFUSE_REFLECTION, "Diffuse RELAX");
    diffuseFilterPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    specularFilterPass = new RELAXPass(mpDevice, mSwapChainSize, RELAX_TYPE::RELAX_SPECULAR, BSDF_LOBE::BSDF_LOBE_GLOSSY_REFLECTION, "Specular RELAX");
    specularFilterPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    deltaReflectionFilterPass = new RELAXPass(mpDevice, mSwapChainSize, RELAX_TYPE::RELAX_DIFFUSE, BSDF_LOBE::BSDF_LOBE_DELTA_REFLECTION, "Delta Reflection RELAX");
    deltaReflectionFilterPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    deltaTransmissionFilterPass = new RELAXPass(mpDevice, mSwapChainSize, RELAX_TYPE::RELAX_DIFFUSE, BSDF_LOBE::BSDF_LOBE_DELTA_TRANSMISSION, "Delta Transmission RELAX");
    deltaTransmissionFilterPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);
    
    residualFilterPass = new RELAXPass(mpDevice, mSwapChainSize, RELAX_TYPE::RELAX_DIFFUSE, BSDF_LOBE::BSDF_LOBE_TRANSMISSION, "Residual RELAX");
    residualFilterPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    //motionVectorPass = new MotionVector(mpDevice, mSwapChainSize);
    //motionVectorPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    //motionVectorSpecularPass = new MotionVectorSpecular(mpDevice, mSwapChainSize);
    //motionVectorSpecularPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    deltaReflectionMotionVectorPass = new MotionVectorDeltaReflection(mpDevice, mSwapChainSize);
    deltaReflectionMotionVectorPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    deltaTransmissionMotionVectorPass = new MotionVectorDeltaTransmission(mpDevice, mSwapChainSize);
    deltaTransmissionMotionVectorPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    modulatePass = new ModulateIllumination(mpDevice, mSwapChainSize);
    modulatePass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    blendPass = new BlendPass(mpDevice, mSwapChainSize);
    blendPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    fxaaPass = new FXAA(mpDevice, mSwapChainSize);
    fxaaPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);

    taaPass = new TAA(mpDevice, mSwapChainSize);
    taaPass->createRenderTextures(mRtvHeap, mpSrvUavHeap);


    tonemapPass = new ToneMapper(mpDevice, mSwapChainSize);

    postProcessQuad = new PostProcessQuad(mpDevice, mpCmdList, mpCmdQueue, mpFence, mFenceValue);

    sceneResourceManager->createSceneCBVs();

    // Create a Log File
    char filename[40];
    struct tm timenow;
    time_t now = time(NULL);
    localtime_s(&timenow , &now);
    strftime(filename, sizeof(filename), "%Y_%m_%d_%H_%M_%S", &timenow);
    logFile = std::ofstream("../Logs/" + std::string(scene->sceneName) + "_" + std::string(filename) + ".txt");

    lastTime = std::chrono::steady_clock::now();


    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 2;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    mpDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui_ImplWin32_Init(winHandle);

    ImGui_ImplDX12_Init(mpDevice, kDefaultSwapChainBuffers,
        DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void RenderApplication::onFrameRender()
{
    uint32_t rtvIndex = beginFrame();
   
    update();
    RenderContext renderContext;
    renderContext.pCmdList = mpCmdList;
    renderContext.pSceneResourceManager = sceneResourceManager;
    renderContext.pSrvUavHeap = mpSrvUavHeap;
    renderContext.restirParam = &restirPass->param;

    RenderData renderDataPathTracer;
    // renderDataPathTracer.gpuHandleDictionary = mpSrvUavHeap->getGPUHandleMap();
    this->pathTracer->forward(&renderContext, renderDataPathTracer);
        
    postProcessQuad->bind(mpCmdList);

    D3D12_GPU_DESCRIPTOR_HANDLE output = mpSrvUavHeap->getGPUHandleByName("gOutputHDR");
    

    if (this->renderMode != 10  && this->renderMode != 0) {
        switch (this->renderMode) {
        case 1: output = mpSrvUavHeap->getGPUHandleByName("gOutputHDR"); break;
        //case 2: output = mpSrvUavHeap->getGPUHandleByName("gDirectIllumination"); break;
        //case 3: output = mpSrvUavHeap->getGPUHandleByName("gIndirectIllumination"); break;

        case 2: output = mpSrvUavHeap->getGPUHandleByName("gDeltaReflectionRadiance"); break;
        case 3: output = mpSrvUavHeap->getGPUHandleByName("gDeltaTransmissionRadiance"); break;
        case 4: output = mpSrvUavHeap->getGPUHandleByName("gResidualRadiance"); break;
        //case 4: output = mpSrvUavHeap->getGPUHandleByName("gDiffuseRadiance"); break;
        //case 5: output = mpSrvUavHeap->getGPUHandleByName("gSpecularRadiance"); break;
        //case 6: output = mpSrvUavHeap->getGPUHandleByName("gEmission"); break;
        case 5: output = mpSrvUavHeap->getGPUHandleByName("gDiffuseReflectance"); break;
        case 6: output = mpSrvUavHeap->getGPUHandleByName("gSpecularReflectance"); break;

        case 7: output = mpSrvUavHeap->getGPUHandleByName("gDeltaReflectionReflectance"); break;
        case 8: output = mpSrvUavHeap->getGPUHandleByName("gDeltaTransmissionReflectance"); break;

        default: output = mpSrvUavHeap->getGPUHandleByName("gOutputHDR"); break;
        }
        //switch (this->renderMode) {
        //case 1: output = mpSrvUavHeap->getGPUHandleByName("gOutputHDR"); break;
        //    //case 2: output = mpSrvUavHeap->getGPUHandleByName("gDirectIllumination"); break;
        //    //case 3: output = mpSrvUavHeap->getGPUHandleByName("gIndirectIllumination"); break;

        //case 2: output = mpSrvUavHeap->getGPUHandleByName("gDeltaReflectionPositionMeshID"); break;
        //case 3: output = mpSrvUavHeap->getGPUHandleByName("gDeltaReflectionNormal"); break;
        //case 4: output = mpSrvUavHeap->getGPUHandleByName("gDeltaTransmissionPositionMeshID"); break;
        //case 5: output = mpSrvUavHeap->getGPUHandleByName("gDeltaTransmissionNormal"); break;
        //case 6: output = mpSrvUavHeap->getGPUHandleByName("gPositionMeshID"); break;
        //case 7: output = mpSrvUavHeap->getGPUHandleByName("gNormal"); break;

        //default: output = mpSrvUavHeap->getGPUHandleByName("gOutputHDR"); break;
        //}
    }
    else
    {
        RenderData depthDerivativeRenderData;

        depthDerivativeRenderData.clear();
        depthDerivativeRenderData.gpuHandleDictionary["gNormalDepth"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormal");
        depthDerivativePass->forward(&renderContext, depthDerivativeRenderData);

        RenderData deltaReflectionMotionVectorRenderData;
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary["gPositionMeshIDPrev"];
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary["gNormalPrev"];
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary["gPositionMeshID"];
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary["gNormal"];
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gDeltaReflectionPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary["gDeltaReflectionPositionMeshID"];
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gDeltaReflectionPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary["gDeltaReflectionPositionMeshIDPrev"];
        deltaReflectionMotionVectorRenderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");

        deltaReflectionMotionVectorPass->forward(&renderContext, deltaReflectionMotionVectorRenderData);

        RenderData deltaTransmissionMotionVectorRenderData;
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary["gPositionMeshIDPrev"];
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary["gNormalPrev"];
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary["gPositionMeshID"];
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary["gNormal"];
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gDeltaTransmissionPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionPositionMeshID");
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gDeltaTransmissionPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionPositionMeshIDPrev");
        deltaTransmissionMotionVectorRenderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");

        deltaTransmissionMotionVectorPass->forward(&renderContext, deltaTransmissionMotionVectorRenderData);


        if (diffuseFilterPass->mEnabled) 
        {
            RenderData renderData;
            renderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPositionMeshID");
            renderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormal");
            renderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPositionMeshIDPrev");
            renderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormalPrev");
            renderData.gpuHandleDictionary["gRadiance"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDiffuseRadiance");
            renderData.gpuHandleDictionary["gMotionVector"] = renderDataPathTracer.outputGPUHandleDictionary.at("gMotionVector");
            renderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");
            renderData.gpuHandleDictionary["gDepthDerivative"] = depthDerivativeRenderData.outputGPUHandleDictionary.at("gDepthDerivative");

            diffuseFilterPass->forward(&renderContext, renderData);
            renderDataPathTracer.outputGPUHandleDictionary["gDiffuseRadiance"] = renderData.outputGPUHandleDictionary["filteredRadiance"];
        }

        if (specularFilterPass->mEnabled)
        {
            RenderData renderData;
            renderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPositionMeshID");
            renderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormal");
            renderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPositionMeshIDPrev");
            renderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormalPrev");
            renderData.gpuHandleDictionary["gRadiance"] = renderDataPathTracer.outputGPUHandleDictionary.at("gSpecularRadiance");
            renderData.gpuHandleDictionary["gMotionVector"] = renderDataPathTracer.outputGPUHandleDictionary.at("gMotionVector");
            renderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");
            renderData.gpuHandleDictionary["gDepthDerivative"] = depthDerivativeRenderData.outputGPUHandleDictionary.at("gDepthDerivative");

            renderData.gpuHandleDictionary["gRoughness"] = renderDataPathTracer.outputGPUHandleDictionary.at("gRoughness");
            // renderData.gpuHandleDictionary["gDeltaReflectionMotionVector"] = deltaReflectionMotionVectorRenderData.outputGPUHandleDictionary.at("gMotionVector");
            renderData.gpuHandleDictionary["gDeltaReflectionNormal"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionNormal");
            renderData.gpuHandleDictionary["gDeltaReflectionPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionPositionMeshID");

            specularFilterPass->forward(&renderContext, renderData);
            renderDataPathTracer.outputGPUHandleDictionary["gSpecularRadiance"] = renderData.outputGPUHandleDictionary["filteredRadiance"];
        }

        if (deltaReflectionFilterPass->mEnabled)
        {
            RenderData renderData;
            renderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionPositionMeshID");
            renderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionNormal");
            renderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionPositionMeshIDPrev");
            renderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionNormalPrev");
            renderData.gpuHandleDictionary["gRadiance"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaReflectionRadiance");
            renderData.gpuHandleDictionary["gMotionVector"] = deltaReflectionMotionVectorRenderData.outputGPUHandleDictionary.at("gMotionVector");
            renderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");
            renderData.gpuHandleDictionary["gDepthDerivative"] = depthDerivativeRenderData.outputGPUHandleDictionary.at("gDepthDerivative");
            
            deltaReflectionFilterPass->forward(&renderContext, renderData);
            renderDataPathTracer.outputGPUHandleDictionary["gDeltaReflectionRadiance"] = renderData.outputGPUHandleDictionary.at("filteredRadiance");
        }

        if (deltaTransmissionFilterPass->mEnabled)
        {
            RenderData renderData;
            renderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionPositionMeshID");
            renderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionNormal");
            renderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionPositionMeshIDPrev");
            renderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionNormalPrev");
            renderData.gpuHandleDictionary["gRadiance"] = renderDataPathTracer.outputGPUHandleDictionary.at("gDeltaTransmissionRadiance");
            renderData.gpuHandleDictionary["gMotionVector"] = deltaTransmissionMotionVectorRenderData.outputGPUHandleDictionary.at("gMotionVector");
            renderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");
            renderData.gpuHandleDictionary["gDepthDerivative"] = depthDerivativeRenderData.outputGPUHandleDictionary.at("gDepthDerivative");

            deltaTransmissionFilterPass->forward(&renderContext, renderData);
            renderDataPathTracer.outputGPUHandleDictionary["gDeltaTransmissionRadiance"] = renderData.outputGPUHandleDictionary.at("filteredRadiance");
        }

        if (residualFilterPass->mEnabled)
        {
            RenderData renderData;
            renderData.gpuHandleDictionary["gPositionMeshID"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPositionMeshID");
            renderData.gpuHandleDictionary["gNormal"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormal");
            renderData.gpuHandleDictionary["gPositionMeshIDPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPositionMeshIDPrev");
            renderData.gpuHandleDictionary["gNormalPrev"] = renderDataPathTracer.outputGPUHandleDictionary.at("gNormalPrev");
            renderData.gpuHandleDictionary["gRadiance"] = renderDataPathTracer.outputGPUHandleDictionary.at("gResidualRadiance");
            renderData.gpuHandleDictionary["gMotionVector"] = renderDataPathTracer.outputGPUHandleDictionary.at("gMotionVector");
            renderData.gpuHandleDictionary["gPathType"] = renderDataPathTracer.outputGPUHandleDictionary.at("gPathType");
            renderData.gpuHandleDictionary["gDepthDerivative"] = depthDerivativeRenderData.outputGPUHandleDictionary.at("gDepthDerivative");

            residualFilterPass->forward(&renderContext, renderData);
            renderDataPathTracer.outputGPUHandleDictionary["gResidualRadiance"] = renderData.outputGPUHandleDictionary.at("filteredRadiance");
        }

        RenderData renderDataModulatePass;

        renderDataModulatePass.gpuHandleDictionary = renderDataPathTracer.outputGPUHandleDictionary;

        modulatePass->forward(&renderContext, renderDataModulatePass);
        output = modulatePass->blendRenderTexture->getGPUSrvHandler();
        

        if (fxaaPass->mEnabled)
        {
            RenderData renderDataBlend;
            renderDataBlend.gpuHandleDictionary["src"] = output;

            fxaaPass->forward(&renderContext, renderDataBlend);

            output = fxaaPass->renderTexture->getGPUSrvHandler();
        }

        if (taaPass->mEnabled)
        {
            RenderData renderDataTAA;
            renderDataTAA.gpuHandleDictionary["gTexColor"] = output;
            renderDataTAA.gpuHandleDictionary["gTexMotionVec"] = renderDataPathTracer.outputGPUHandleDictionary.at("gMotionVector");
            // renderDataBlend.gpuHandleDictionary["gTexPrevColor"] = output;

            taaPass->forward(&renderContext, renderDataTAA);

            output = renderDataTAA.outputGPUHandleDictionary.at("gFilteredTexColor");
        }
    }

    RenderData renderDataTonemap;
    // renderDataTonemap.gpuHandleDictionary["src"] = svgfPass->temporalAccumulationTextureDirect->getGPUSrvHandler();
    // renderDataTonemap.gpuHandleDictionary["src"] = svgfPass->motionVectorRenderTexture->getGPUSrvHandler();
    // renderDataTonemap.gpuHandleDictionary["src"] = svgfPass->waveletIndirectPingPong2->getGPUSrvHandler();

    renderDataTonemap.gpuHandleDictionary["src"] = output;
    // renderDataTonemap.gpuHandleDictionary["src"] = mpSrvUavHeap->getGPUHandleByName("gOutputHDR");

    // output; // mpSrvUavHeap->getGPUHandleByName("gOutputHDR");
    renderDataTonemap.cpuHandleDictionary["dst"] = mFrameObjects[rtvIndex].rtvHandle;
    renderDataTonemap.resourceDictionary["dst"] = mFrameObjects[rtvIndex].pSwapChainBuffer;

    this->tonemapPass->forward(&renderContext, renderDataTonemap);

    // resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    // mpCmdList->CopyResource(mFrameObjects[rtvIndex].pSwapChainBuffer, renderDataPathTracer.resourceDictionary.at("gOutput"));

    //this->pathTracer->copyback(mpCmdList);
    pathTracer->copyback(mpCmdList);


    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    ImGui::Begin("Path Tracer Settings");                          // Create a window called "Hello, world!" and append into it.
    
    pathTracer->processGUI();
    restirPass->processGUI();
    if(svgfPass)
        svgfPass->processGUI();
    
    if (diffuseFilterPass)
        diffuseFilterPass->processGUI();
    if (specularFilterPass)
        specularFilterPass->processGUI();
    if (deltaReflectionFilterPass)
        deltaReflectionFilterPass->processGUI();
    if (deltaTransmissionFilterPass)
        deltaTransmissionFilterPass->processGUI();
    if (residualFilterPass)
        residualFilterPass->processGUI();

    modulatePass->processGUI();
    tonemapPass->processGUI();
    blendPass->processGUI();
    fxaaPass->processGUI();
    taaPass->processGUI();

    mDirty = (pathTracer->mDirty) || (svgfPass && svgfPass->mDirty) || (diffuseFilterPass && diffuseFilterPass->mDirty) || (restirPass->mDirty);

    //ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
    //ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
    //ImGui::Checkbox("Another Window", &show_another_window);
    //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    //ImGui::CollapsingHeader("Help");
    //if (ImGui::CollapsingHeader("Window options"))
    //{
    //    ImGui::Text("USER GUIDE:");
    //    ImGui::Text("USER GUIDE3:");
    //}

    ImGui::End();

    ImGui::Render();

    //// Render Dear ImGui graphics

    //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ////const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
    ////mpCmdList->ClearRenderTargetView(mFrameObjects[rtvIndex].rtvHandle, clear_color_with_alpha, 0, nullptr);
    mpCmdList->OMSetRenderTargets(1, &mFrameObjects[rtvIndex].rtvHandle, FALSE, nullptr);
    // Bind the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { g_pd3dSrvDescHeap };
    mpCmdList->SetDescriptorHeaps(arraysize(heaps), heaps);
    // mpCmdList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mpCmdList);

    //barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    //barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    //g_pd3dCommandList->ResourceBarrier(1, &barrier);
    //g_pd3dCommandList->Close();

    
    endFrame(rtvIndex);
}



void RenderApplication::onShutdown()
{
    // Wait for the command queue to finish execution
    mFenceValue++;
    mpCmdQueue->Signal(mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);

    logFile.close();
}

void RenderApplication::update()
{
    HRESULT result = mpKeyboard->GetDeviceState(256, mpKeyboardState);

    uint nextRenderMode = renderMode;
    for(int i = 0; i < 10; i++) {
        if (mpKeyboardState[DIK_1 + i] & 0x80) {
            nextRenderMode = i + 1;
        }
    }
    if (mpKeyboardState[DIK_P] & 0x80) {
        doPostProcess = true;
    }
    else if (mpKeyboardState[DIK_O] & 0x80) {
        doPostProcess = false;
    }

    PerFrameData frameData;
    Sensor* camera = scene->getSensor();
    mat4 projection = perspective(radians(camera->fovy), (float)camera->width / (float)camera->height, 0.1f, 100.0f);
    mat4 view = lookAt(camera->position, camera->position + camera->forward, camera->up);
    mat4 projview = projection * view;
    frameData.previousCameraPosition = vec4(camera->position, 1.0f);
    frameData.previousProjView = transpose(projview);
    frameData.renderMode = nextRenderMode;

    
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float elapsedTimeMicrosec = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
    // logFile << std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count() << "\n";

    lastTime = now;

    float elapsedTimeSec = elapsedTimeMicrosec * 1e-6;
    float moveSpeed = 1.5f;
    float rotationSpeed = 30.0f;

    if (mpKeyboardState[DIK_LSHIFT] & 0x80) {
        moveSpeed *= 4.0f;
        rotationSpeed *= 2.0f;
    }

    if (mpKeyboardState[DIK_LCONTROL] & 0x80) {
        moveSpeed /= 10.0f;
        rotationSpeed /= 10.0f;
    }

    bool cameraMoved = false;
    if (mpKeyboardState[DIK_W] & 0x80) {
        camera->position += moveSpeed * elapsedTimeSec * camera->forward;
        cameraMoved = true;
    }
    else if (mpKeyboardState[DIK_S] & 0x80) {
        camera->position -= moveSpeed * elapsedTimeSec * camera->forward;
        cameraMoved = true;
    }
    if (mpKeyboardState[DIK_A] & 0x80) {
        camera->position -= moveSpeed * elapsedTimeSec * camera->right;
        cameraMoved = true;
    }
    else if (mpKeyboardState[DIK_D] & 0x80) {
        camera->position += moveSpeed * elapsedTimeSec * camera->right;
        cameraMoved = true;
    }
    if (mpKeyboardState[DIK_Q] & 0x80) {
        camera->position += moveSpeed * elapsedTimeSec * camera->up;
        cameraMoved = true;
    }
    else if (mpKeyboardState[DIK_E] & 0x80) {
        camera->position -= moveSpeed * elapsedTimeSec * camera->up;
        cameraMoved = true;
    }

    
    if (mpKeyboardState[DIK_UPARROW] & 0x80) {
        glm::quat rot = glm::angleAxis(glm::radians(rotationSpeed * elapsedTimeSec), camera->right);
        glm::mat3 rotMatrix = glm::mat3_cast(rot);
        camera->up = rot * camera->up;
        camera->forward = rot * camera->forward;
        cameraMoved = true;
    }

    if (mpKeyboardState[DIK_DOWNARROW] & 0x80) {
        glm::quat rot = glm::angleAxis(glm::radians(-rotationSpeed * elapsedTimeSec), camera->right);
        glm::mat3 rotMatrix = glm::mat3_cast(rot);
        camera->up = rot * camera->up;
        camera->forward = rot * camera->forward;
        cameraMoved = true;
    }

    if (mpKeyboardState[DIK_RIGHTARROW] & 0x80) {
        glm::quat rot = glm::angleAxis(glm::radians(-rotationSpeed * elapsedTimeSec), vec3(0,1,0));
        glm::mat3 rotMatrix = glm::mat3_cast(rot);
        camera->up = rot * camera->up;
        camera->forward = rot * camera->forward;
        camera->right = rot * camera->right;
        cameraMoved = true;
    }

    if (mpKeyboardState[DIK_LEFTARROW] & 0x80) {
        glm::quat rot = glm::angleAxis(glm::radians(rotationSpeed * elapsedTimeSec), vec3(0, 1, 0));
        glm::mat3 rotMatrix = glm::mat3_cast(rot);
        camera->up = rot * camera->up;
        camera->forward = rot * camera->forward;
        camera->right = rot * camera->right;
        cameraMoved = true;
    }

    if (cameraMoved || renderMode != nextRenderMode || mDirty) {
        this->mFrameNumber = 1;
    }
    else {
        this->mFrameNumber += 1;
    }
    this->mTotalFrameNumber += 1;
    renderMode = nextRenderMode;

    camera->update();

    frameData.u = vec4(camera->u, 1.0f);
    frameData.v = vec4(camera->v, 1.0f);
    frameData.w = vec4(camera->w, 1.0f);
    frameData.cameraPosition = vec4(camera->position, 1.0f);
    frameData.paramChanged = mDirty;
    frameData.cameraChanged = cameraMoved;

    frameData.frameNumber = mFrameNumber;
    frameData.totalFrameNumber = mTotalFrameNumber;

    frameData.lightNumber = scene->lights.size();
    frameData.envMapTransform = transpose((scene->envMapTransform));

    this->sceneResourceManager->update(frameData);

    elapsedTime[mTotalFrameNumber % FRAME_ACCUMULATE_NUMBER] = elapsedTimeSec;

    float averageElapsedTime = 0;
    for (int i = 0; i < FRAME_ACCUMULATE_NUMBER; i++) {
        averageElapsedTime += elapsedTime[i];
    }
    float FPS = FRAME_ACCUMULATE_NUMBER / averageElapsedTime;

    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << FPS;
    std::string FPSs = stream.str();

    std::stringstream stream2;
    stream2 << std::fixed << std::setprecision(2) << elapsedTimeMicrosec / 1000;
    std::string ElapsedTimes = stream2.str();

    std::string windowText = "FPS:" + FPSs + " Current Frame: " + ElapsedTimes + " ms" + " Accum Frame: " + std::to_string(mFrameNumber) + " PostProcess: " + std::to_string(doPostProcess) + " Render Mode: " + std::to_string(renderMode);
    SetWindowTextA(mHwnd, windowText.c_str());
}