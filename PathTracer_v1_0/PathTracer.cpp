#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include "PathTracer.h"
#include "DX12Initializer.h"
#include "DX12Helper.h"
// #include "AccelerationStructureHelper.h"
#include "DxilLibrary.h"
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


void TutorialPathTracer::initDX12(HWND winHandle, uint32_t winWidth, uint32_t winHeight)
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

uint32_t TutorialPathTracer::beginFrame()
{
    // Bind the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { mSrvUavHeap->getDescriptorHeap() };
    mpCmdList->SetDescriptorHeaps(arraysize(heaps), heaps);
    return mpSwapChain->GetCurrentBackBufferIndex();
}

void TutorialPathTracer::endFrame(uint32_t rtvIndex)
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


void TutorialPathTracer::createRtPipelineState()
{
    // Need 10 subobjects:
    //  1 for the DXIL library
    //  1 for hit-group
    //  2 for RayGen root-signature (root-signature and the subobject association)
    //  2 for the root-signature shared between miss and hit shaders (signature and association)
    //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
    //  1 for pipeline config
    //  1 for the global root signature
    std::array<D3D12_STATE_SUBOBJECT, 13> subobjects;
    uint32_t index = 0;

    // Create the DXIL library
    DxilLibrary dxilLib = createDxilLibrary();
    subobjects[index++] = dxilLib.stateSubobject; // 0 Library

    HitProgram hitProgram(nullptr, kClosestHitShader, kHitGroup);
    subobjects[index++] = hitProgram.subObject; // 1 Hit Group
        
    // Create the shadow-ray hit group
    HitProgram shadowHitProgram(nullptr, kShadowChs, kShadowHitGroup);
    subobjects[index++] = shadowHitProgram.subObject; // 2 Shadow Hit Group

    // Create the ray-gen root-signature and association
    LocalRootSignature rgsRootSignature(mpDevice, createRayGenRootDesc().desc);
    subobjects[index] = rgsRootSignature.subobject; // 3 RayGen Root Sig

    uint32_t rgsRootIndex = index++; // 3
    ExportAssociation rgsRootAssociation(&kRayGenShader, 1, &(subobjects[rgsRootIndex]));
    subobjects[index++] = rgsRootAssociation.subobject; // 4 Associate Root Sig to RGS

    // Create the hit root-signature and association
    LocalRootSignature hitRootSignature(mpDevice, createHitRootDesc().desc);
    subobjects[index] = hitRootSignature.subobject; //  5 Hit Root Sig

    uint32_t hitRootIndex = index++; // 5
    ExportAssociation hitRootAssociation(&kClosestHitShader, 1, &(subobjects[hitRootIndex]));
    subobjects[index++] = hitRootAssociation.subobject; // 6 Associate Hit Root Sig to Hit Group

    // Create the miss root-signature and association
    D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
    emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    LocalRootSignature missRootSignature(mpDevice, emptyDesc);
    subobjects[index] = missRootSignature.subobject; // 7 Miss Root Sig

    uint32_t emptyRootIndex = index++; // 7
    const WCHAR* emptyRootExport[] = { kMissShader, kMissEnvShader , kShadowChs, kShadowMiss };
    ExportAssociation missRootAssociation(emptyRootExport, arraysize(emptyRootExport), &(subobjects[emptyRootIndex]));
    subobjects[index++] = missRootAssociation.subobject; // 8 Associate Miss Root Sig to Miss Shader

    // Bind the payload size to the programs
    ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 38);
    subobjects[index] = shaderConfig.subobject; // 9 Shader Config

    uint32_t shaderConfigIndex = index++; // 9
    const WCHAR* shaderExports[] = { kMissShader, kMissEnvShader, kClosestHitShader, kRayGenShader,kShadowMiss, kShadowChs };
    ExportAssociation configAssociation(shaderExports, arraysize(shaderExports), &(subobjects[shaderConfigIndex]));
    subobjects[index++] = configAssociation.subobject; // 10 Associate Shader Config to Miss, CHS, RGS

    // Create the pipeline config
    PipelineConfig config(1);
    subobjects[index++] = config.subobject; // 11

    // Create the global root signature and store the empty signature
    GlobalRootSignature root(mpDevice, createGlobalRootDesc().desc);
    mpEmptyRootSig = root.pRootSig;
    subobjects[index++] = root.subobject; // 12

    // Create the state
    D3D12_STATE_OBJECT_DESC desc;
    desc.NumSubobjects = index; // 13
    desc.pSubobjects = subobjects.data();
    desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

    d3d_call(mpDevice->CreateStateObject(&desc, IID_PPV_ARGS(&mpPipelineState)));
}

void TutorialPathTracer::createShaderTable()
{
    /** The shader-table layout is as follows:
        Entry 0 - Ray-gen program
        Entry 1 - Miss program
        Entry 2 - Hit program
        All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
        The ray-gen program requires the largest entry - sizeof(program identifier) + 8 bytes for a descriptor-table.
        The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
    */

    // Calculate the size and create the buffer
    mShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    mShaderTableEntrySize += 8; // The ray-gen's descriptor table
    mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableEntrySize);
    uint32_t nMeshes = this->scene->getMeshes().size();
    uint32_t shaderTableSize = mShaderTableEntrySize * (3 + 2 * nMeshes);

    // For simplicity, we create the shader-table on the upload heap. You can also create it on the default heap
    mpShaderTable = createBuffer(mpDevice, shaderTableSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    // Map the buffer
    uint8_t* pData;
    d3d_call(mpShaderTable->Map(0, nullptr, (void**)&pData));

    MAKE_SMART_COM_PTR(ID3D12StateObjectProperties);
    ID3D12StateObjectPropertiesPtr pRtsoProps;
    mpPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

    // Entry 0 - ray-gen program ID and descriptor data
    memcpy(pData, pRtsoProps->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    uint64_t heapStart = mSrvUavHeap->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart().ptr;
    *(uint64_t*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart;

    // Entry 1 - miss program
    if (scene->envMapTexture) {
        memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissEnvShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
    else {
        memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
    
    // Entry 2 - shadow-ray miss
    memcpy(pData + mShaderTableEntrySize * 2, pRtsoProps->GetShaderIdentifier(kShadowMiss), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    
    // Entries 3- - The mesh hit program. ProgramID and constant-buffer data
    for (uint32_t i = 0; i < nMeshes; i++)
    {
        // Closest hit for primary ray
        uint8_t* pHitEntry = pData + mShaderTableEntrySize * (2 * i + 3); // +2 skips the ray-gen and miss entries
        memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        uint8_t* pCbDesc = pHitEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;            // The location of the root-descriptor
        assert(((uint64_t)pCbDesc % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
        
        Mesh& mesh = scene->meshes[i];
        std::string meshRefID = scene->meshRefID[i];
        int bsdfID = scene->materialIDDictionary[meshRefID];
        BSDF* meshBSDF = scene->bsdfs[bsdfID];

        if (meshBSDF->diffuseReflectanceTexturePath.length() > 0) {
            int textureID = scene->textureIDDictionary[meshBSDF->diffuseReflectanceTexturePath];
            Texture* texture = scene->textures[textureID];
            *(uint64_t*)(pCbDesc) = mSrvUavHeap->getGPUHandle(texture->descriptorHandleOffset).ptr;
        }
        else {
            *(uint64_t*)(pCbDesc) = mpTextureStartHandle.ptr;
        }

        // Closest hit for shadow ray
        uint8_t* pEntryShadow = pData + mShaderTableEntrySize * (2 * i + 4);
        memcpy(pEntryShadow, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }

    // Unmap
    mpShaderTable->Unmap(0, nullptr);
}


void TutorialPathTracer::createUAVBuffer(DXGI_FORMAT format, std::string name, uint depth)
{
    ID3D12ResourcePtr outputResources;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.DepthOrArraySize = depth;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Format = format;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resDesc.Width = mSwapChainSize.x;
    resDesc.Height = mSwapChainSize.y;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    d3d_call(mpDevice->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&outputResources))); // Starting as copy-source to simplify onFrameRender()

    if (depth == 1) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        mpDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, mSrvUavHeap->addDescriptorHandle(name.c_str()));
    }
    else {
        for (int i = 0; i < depth; i++) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Format = format;
            uavDesc.Texture2DArray.ArraySize = 1;
            uavDesc.Texture2DArray.FirstArraySlice = UINT(i);
            uavDesc.Texture2DArray.PlaneSlice = 0;
            mpDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, mSrvUavHeap->addDescriptorHandle(name.c_str()));
        }
    }
    outputUAVBuffers[name] = outputResources;
    //int HeapOffset = (srvHandle.ptr - mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr) / mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}


void TutorialPathTracer::createShaderResources()
{
    // Create an SRV/UAV descriptor heap
    mSrvUavHeap = new HeapData(mpDevice, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    sceneResourceManager = new SceneResourceManager(scene, mpDevice, mSrvUavHeap);
    sceneResourceManager->createSceneAccelerationStructure(mpCmdQueue, mFrameObjects[0].pCmdAllocator, mpCmdList, mpFence, mFenceEvent, mFenceValue);

    // 1. Create the output resource. The dimensions and format should match the swap-chain
    createUAVBuffer(DXGI_FORMAT_R8G8B8A8_UNORM, "gOutput");

    // 2. Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = sceneResourceManager->getToplevelAS()->GetGPUVirtualAddress();
    mpDevice->CreateShaderResourceView(nullptr, &srvDesc, mSrvUavHeap->addDescriptorHandle("AccelerationStructure"));

    // 3~6 scene material, geometry
    sceneResourceManager->createSceneSRVs();

    // 7. HDR
    createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gOutputHDR");

    // 8. Direct Illumination
    createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gDirectIllumination");

    // 9. Indirect Illumination
    createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gIndirectIllumination");

    // 10. Reflectance
    createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gReflectance");

    // 11. Position / ID
    createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gPositionMeshID");

    // 12. Normal
    createUAVBuffer(DXGI_FORMAT_R8G8B8A8_SNORM, "gNormal");

    // 13, 14 Prev buffer
    createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gPositionMeshIDPrev");
    createUAVBuffer(DXGI_FORMAT_R8G8B8A8_SNORM, "gNormalPrev");

    mpTextureStartHandle = mSrvUavHeap->getLastGPUHandle();
    sceneResourceManager->createSceneTextureResources(mpCmdQueue, mFrameObjects[0].pCmdAllocator, mpCmdList, mpFence, mFenceEvent, mFenceValue);
}


//////////////////////////////////////////////////////////////////////////
// Callbacks
//////////////////////////////////////////////////////////////////////////
void TutorialPathTracer::onLoad(HWND winHandle, uint32_t winWidth, uint32_t winHeight)
{

    initDX12(winHandle, winWidth, winHeight);
    
    // init input
    DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)(&mpInput), nullptr);
    mpInput->CreateDevice(GUID_SysKeyboard, &mpKeyboard, nullptr);
    mpKeyboard->SetDataFormat(&c_dfDIKeyboard);
    mpKeyboard->Acquire();


    createRtPipelineState();
    createShaderResources();
    createShaderTable();

    initPostProcess();

    mpLightParametersBuffer = createBuffer(mpDevice, sizeof(LightParameter) * 20, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    d3d_call(mpLightParametersBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, scene->lights.data(), sizeof(LightParameter) * scene->lights.size());
    mpLightParametersBuffer->Unmap(0, nullptr);

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

void TutorialPathTracer::onFrameRender()
{
    uint32_t rtvIndex = beginFrame();
    ID3D12ResourcePtr mpOutputResource = outputUAVBuffers["gOutput"];

    update();

    // Let's raytrace
    resourceBarrier(mpCmdList, mpOutputResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    resourceBarrier(mpCmdList, outputUAVBuffers["gDirectIllumination"], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    resourceBarrier(mpCmdList, outputUAVBuffers["gIndirectIllumination"], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    resourceBarrier(mpCmdList, outputUAVBuffers["gPositionMeshID"], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    resourceBarrier(mpCmdList, outputUAVBuffers["gNormal"], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
    raytraceDesc.Width = mSwapChainSize.x;
    raytraceDesc.Height = mSwapChainSize.y;
    raytraceDesc.Depth = 1;

    // RayGen is the first entry in the shader-table
    raytraceDesc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress() + 0 * mShaderTableEntrySize;
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

    // Miss is the second entry in the shader-table
    size_t missOffset = 1 * mShaderTableEntrySize;
    raytraceDesc.MissShaderTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + missOffset;
    raytraceDesc.MissShaderTable.StrideInBytes = mShaderTableEntrySize;
    raytraceDesc.MissShaderTable.SizeInBytes = mShaderTableEntrySize * 2;   // Only a s single miss-entry

    // Hit is the third entry in the shader-table
    size_t hitOffset = 3 * mShaderTableEntrySize;
    raytraceDesc.HitGroupTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + hitOffset;
    raytraceDesc.HitGroupTable.StrideInBytes = mShaderTableEntrySize;
    raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize * ( 2 * scene->getMeshes().size());

    // Bind the empty root signature
    mpCmdList->SetComputeRootSignature(mpEmptyRootSig);
    mpCmdList->SetComputeRootConstantBufferView(0, mpCameraConstantBuffer->GetGPUVirtualAddress());
    mpCmdList->SetComputeRootConstantBufferView(1, mpLightParametersBuffer->GetGPUVirtualAddress());


    // mpSrvUavHeap 2,3,4,5
    mpCmdList->SetComputeRootDescriptorTable(2, sceneResourceManager->getSRVStartHandle());//2 at createGlobalRootDesc

    // UAV
    mpCmdList->SetComputeRootDescriptorTable(3, mSrvUavHeap->getGPUHandleByName("gOutputHDR"));//3 at createGlobalRootDesc

    // Dispatch
    mpCmdList->SetPipelineState1(mpPipelineState.GetInterfacePtr());
    mpCmdList->DispatchRays(&raytraceDesc);

    //Copy the results to the back-buffer
    resourceBarrier(mpCmdList, mpOutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, outputUAVBuffers["gDirectIllumination"], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, outputUAVBuffers["gIndirectIllumination"], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, outputUAVBuffers["gPositionMeshID"], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, outputUAVBuffers["gNormal"], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    if (doPostProcess) {
        postProcess(rtvIndex);
    }
    else {
        resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
        mpCmdList->CopyResource(mFrameObjects[rtvIndex].pSwapChainBuffer, mpOutputResource);
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    //D3D12_RESOURCE_BARRIER barrier = {};
    //barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    //barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    //barrier.Transition.pResource = mFrameObjects[rtvIndex].pSwapChainBuffer;
    //barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    //barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    //barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    
    //mFrameObjects[bufferIndex].pCmdAllocator->Reset();
    // mpCmdList->Reset(mFrameObjects[bufferIndex].pCmdAllocator, nullptr);
    //mpCmdList->SetPipelineState(nullptr);
    // mpCmdList->ResourceBarrier(1, &barrier);
    resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

    svgfPass->processGUI();

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

void TutorialPathTracer::initPostProcess()
{

    this->defaultCopyShader = new Shader(L"QuadVertexShader.hlsl", L"CopyShader.hlsl", mpDevice, 1);

    postProcessQuad = new PostProcessQuad(mpDevice, mpCmdList, mpCmdQueue, mpFence, mFenceValue);

    svgfPass = new SVGFPass(mpDevice, mSwapChainSize);
    svgfPass->createRenderTextures(mRtvHeap, mSrvUavHeap);

    tonemapPass = new ToneMapper(mpDevice, mSwapChainSize);
    // tonemapPass->createRenderTextures(mRtvHeap, mSrvUavHeap);
}

void TutorialPathTracer::postProcess(int rtvIndex)
{
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE> & gpuHandlesMap = mSrvUavHeap->getGPUHandleMap();

    postProcessQuad->bind(mpCmdList);
    svgfPass->setViewPort(mpCmdList);
    svgfPass->forward(mpCmdList, gpuHandlesMap, outputUAVBuffers, mpCameraConstantBuffer);

    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle = svgfPass->reconstructionRenderTexture->getGPUSrvHandler();
    // D3D12_GPU_DESCRIPTOR_HANDLE inputHandle = svgfPass->waveletDirect[4]->getGPUSrvHandler();

    tonemapPass->forward(mpCmdList, inputHandle, mFrameObjects[rtvIndex].rtvHandle, mFrameObjects[rtvIndex].pSwapChainBuffer);

    resourceBarrier(mpCmdList, outputUAVBuffers["gPositionMeshID"], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, outputUAVBuffers["gPositionMeshIDPrev"], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(outputUAVBuffers["gPositionMeshIDPrev"], outputUAVBuffers["gPositionMeshID"]);
    resourceBarrier(mpCmdList, outputUAVBuffers["gPositionMeshID"], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
    resourceBarrier(mpCmdList, outputUAVBuffers["gNormal"], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, outputUAVBuffers["gNormalPrev"], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(outputUAVBuffers["gNormalPrev"], outputUAVBuffers["gNormal"]);
    resourceBarrier(mpCmdList, outputUAVBuffers["gNormal"], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}


void TutorialPathTracer::onShutdown()
{
    // Wait for the command queue to finish execution
    mFenceValue++;
    mpCmdQueue->Signal(mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);

    logFile.close();
}

void TutorialPathTracer::update()
{
    HRESULT result = mpKeyboard->GetDeviceState(256, mpKeyboardState);

    uint nextRenderMode = renderMode;
    for(int i = 0; i < 10; i++) {
        if (mpKeyboardState[DIK_1 + i] & 0x80) {
            nextRenderMode = i;
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
    frameData.previousProjView = transpose(projview);
    frameData.renderMode = nextRenderMode;

    
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float elapsedTimeMicrosec = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
    // logFile << std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count() << "\n";

    lastTime = now;

    float elapsedTimeSec = elapsedTimeMicrosec * 1e-6;
    float moveSpeed = 3.0f;
    float rotationSpeed = 30.0f;

    if (mpKeyboardState[DIK_LSHIFT] & 0x80) {
        moveSpeed *= 4.0f;
        rotationSpeed *= 2.0f;
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

    if (cameraMoved || renderMode != nextRenderMode) {
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
    frameData.frameNumber = mFrameNumber;
    frameData.totalFrameNumber = mTotalFrameNumber;

    frameData.lightNumber = scene->lights.size();
    frameData.envMapTransform = transpose((scene->envMapTransform));
    if (!this->mpCameraConstantBuffer)
        mpCameraConstantBuffer = createBuffer(mpDevice, sizeof(PerFrameData), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    d3d_call(mpCameraConstantBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &frameData, sizeof(PerFrameData));
    mpCameraConstantBuffer->Unmap(0, nullptr);


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

    std::string windowText = "FPS:" + FPSs + " Current Frame: " + ElapsedTimes + " ms" + " Accum Frame: " + std::to_string(mFrameNumber) + " PostProcess: " + std::to_string(doPostProcess);
    SetWindowTextA(mHwnd, windowText.c_str());
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    //Scene* scene = new Scene("material-testball");
    // Scene* scene = new Scene("cornell-box");
    //Scene* scene = new Scene("kitchen");
    //Scene* scene = new Scene("living-room-2");
    Scene* scene = new Scene("staircase");

    TutorialPathTracer pathtracer = TutorialPathTracer();
    pathtracer.setScene(scene);
    Framework::run(pathtracer, "Tutorial PathTracer", scene->getSensor()->width, scene->getSensor()->height);
}


void main()
{
    Scene* scene = new Scene("veach-ajar");
}