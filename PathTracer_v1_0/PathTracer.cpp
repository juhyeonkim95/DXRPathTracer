#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include "PathTracer.h"
#include "DX12Initializer.h"
#include "DX12Helper.h"
#include "AccelerationStructureHelper.h"
#include "DxilLibrary.h"
#include "CommonStruct.h"
#include "DirectXTex.h"
#include "DirectXTex.inl"
#include "d3dx12.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>

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
    mRtvHeap.pHeap = createDescriptorHeap(mpDevice, kRtvHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    // Create the per-frame objects
    for (uint32_t i = 0; i < kDefaultSwapChainBuffers; i++)
    {
        d3d_call(mpDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mFrameObjects[i].pCmdAllocator)));
        d3d_call(mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&mFrameObjects[i].pSwapChainBuffer)));
        mFrameObjects[i].rtvHandle = createRTV(mpDevice, mFrameObjects[i].pSwapChainBuffer, mRtvHeap.pHeap, mRtvHeap.usedEntries, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    // Create the command-list
    d3d_call(mpDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrameObjects[0].pCmdAllocator, nullptr, IID_PPV_ARGS(&mpCmdList)));

    // Create a fence and the event
    d3d_call(mpDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mpFence)));
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    //RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM);
    //std::unique_ptr<BasicPostProcess> postProcess = std::make_unique<BasicPostProcess>(mpDevice, rtState, BasicPostProcess::Sepia);
}

uint32_t TutorialPathTracer::beginFrame()
{
    // Bind the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { mpSrvUavHeap };
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

AccelerationStructureBuffers TutorialPathTracer::createTopLevelAccelerationStructure()
{
    size_t numBottomLevelAS = mpBottomLevelAS.size();
    // First, get the size of the TLAS buffers and create them
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = numBottomLevelAS;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    mpDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers
    AccelerationStructureBuffers buffers;
    buffers.pScratch = createBuffer(mpDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.pResult = createBuffer(mpDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
    mTlasSize = info.ResultDataMaxSizeInBytes;

    // The instance desc should be inside a buffer, create and map the buffer
    buffers.pInstanceDesc = createBuffer(mpDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * numBottomLevelAS, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    buffers.pInstanceDesc->Map(0, nullptr, (void**)&instanceDescs);
    ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * numBottomLevelAS);

    for (uint32_t i = 0; i < numBottomLevelAS; i++) {
        // Initialize the instance desc. We only have a single instance
        instanceDescs[i].InstanceID = i;                            // This value will be exposed to the shader via InstanceID()
        instanceDescs[i].InstanceContributionToHitGroupIndex = 2 * i;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        mat4 m = scene->meshes[i].transform;
        m = transpose(m);
        memcpy(instanceDescs[i].Transform, &m, sizeof(instanceDescs[i].Transform));
        instanceDescs[i].AccelerationStructure = mpBottomLevelAS[i]->GetGPUVirtualAddress();
        instanceDescs[i].InstanceMask = 0xFF;
    }

    // Unmap
    buffers.pInstanceDesc->Unmap(0, nullptr);

    // Create the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
    asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

    mpCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    mpCmdList->ResourceBarrier(1, &uavBarrier);

    return buffers;
}

void TutorialPathTracer::createAccelerationStructures()
{
    mpBottomLevelAS.resize(this->scene->getMeshes().size());
    std::vector<AccelerationStructureBuffers> bottomLevelBuffers;
    for (int i = 0; i < scene->getMeshes().size(); i++) {
        AccelerationStructureBuffers bottomLevelBuffer = createBottomLevelAS(mpDevice, mpCmdList, (scene->getMeshes())[i]);
        bottomLevelBuffers.push_back(bottomLevelBuffer);
        mpBottomLevelAS[i] = bottomLevelBuffer.pResult;
    }

    AccelerationStructureBuffers topLevelBuffers = this->createTopLevelAccelerationStructure();
    //createTopLevelAS(mpDevice, mpCmdList, mpBottomLevelAS, mTlasSize);

    // TODO : Flush(?)
    // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
    mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);
    uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();
    mpCmdList->Reset(mFrameObjects[bufferIndex].pCmdAllocator, nullptr);

    // Store the AS buffers. The rest of the buffers will be released once we exit the function
    mpTopLevelAS = topLevelBuffers.pResult;
    //mpBottomLevelAS = bottomLevelBuffers.pResult;
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
    uint64_t heapStart = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr;
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
        uint8_t* pHitEntry = pData + mShaderTableEntrySize * (2 * i + 3); // +2 skips the ray-gen and miss entries
        memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        uint8_t* pCbDesc = pHitEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8;            // The location of the root-descriptor
        assert(((uint64_t)pCbDesc % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address

        D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        
        Mesh& mesh = scene->meshes[i];
        std::string meshRefID = scene->meshRefID[i];
        int bsdfID = scene->materialIDDictionary[meshRefID];
        BSDF* meshBSDF = scene->bsdfs[bsdfID];

        if (meshBSDF->diffuseReflectanceTexturePath.length() > 0) {
            int textureID = scene->textureIDDictionary[meshBSDF->diffuseReflectanceTexturePath];
            Texture* texture = scene->textures[textureID];
            uint32_t descriptorOffset = texture->descriptorHandleOffset;
            descriptorHandle.ptr += descriptorOffset * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            *(uint64_t*)(pCbDesc) = descriptorHandle.ptr;
        } else {
            descriptorHandle.ptr += (textureStartHeapOffset) * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            *(uint64_t*)(pCbDesc) = descriptorHandle.ptr;
        }

        uint8_t* pCbDescAccel = pHitEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle2 = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        descriptorHandle2.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        *(uint64_t*)(pCbDescAccel) = descriptorHandle2.ptr;


        uint8_t* pEntryShadow = pData + mShaderTableEntrySize * (2 * i + 4);
        memcpy(pEntryShadow, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }

    // Unmap
    mpShaderTable->Unmap(0, nullptr);
}

void TutorialPathTracer::createSRVTexture(DXGI_FORMAT format, std::string name) {
    auto const heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format,
        static_cast<UINT64>(mSwapChainSize.x),
        static_cast<UINT>(mSwapChainSize.y),
        1, 1, 1, 0, D3D12_RESOURCE_FLAG_NONE);

    ID3D12ResourcePtr outputResources;
    // Create a render target
    mpDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outputResources));

    outputUAVBuffers[name] = outputResources;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    // Create SRV.
    mpDevice->CreateShaderResourceView(outputResources, &srvDesc, srvHandle);

    int HeapOffset = (srvHandle.ptr - mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr) / mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mSrvHeapIndexMap[name] = HeapOffset;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += (HeapOffset) * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    gpuHandlesMap[name] = gpuHandle;

    srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

        mpDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, srvHandle);
        srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    else {
        for (int i = 0; i < depth; i++) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Format = format;
            uavDesc.Texture2DArray.ArraySize = 1;
            uavDesc.Texture2DArray.FirstArraySlice = UINT(i);
            uavDesc.Texture2DArray.PlaneSlice = 0;
            mpDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, srvHandle);
            srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    }
    outputUAVBuffers[name] = outputResources;
    int HeapOffset = (srvHandle.ptr - mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr) / mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mSrvHeapIndexMap[name] = HeapOffset - 1;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += (HeapOffset - 1) * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    gpuHandlesMap[name] = gpuHandle;
}

void TutorialPathTracer::createShaderResources()
{
    // Create an SRV/UAV descriptor heap
    mpSrvUavHeap = createDescriptorHeap(mpDevice, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    srvHandle = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // 1. Create the output resource. The dimensions and format should match the swap-chain
    createUAVBuffer(DXGI_FORMAT_R8G8B8A8_UNORM, "output");

    // 2. Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS->GetGPUVirtualAddress();
    mpDevice->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
    srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 3. Material Data
    {
        std::vector<Material> materialData;
        for (BSDF* bsdf : scene->bsdfs)
        {
            Material material;
            material.materialType = bsdf->bsdfType;
            material.isDoubleSided = bsdf->isDoubleSided;

            material.diffuseReflectance = bsdf->diffuseReflectance;
            material.diffuseReflectanceTextureID = bsdf->diffuseReflectanceTexturePath.length() > 0 ? 1 : 0;
            material.specularReflectance = bsdf->specularReflectance;
            //material.specularReflectanceTextureID = bsdf->diffuseReflectanceTexturePath.length() > 0 ? 1 : 0;
            material.specularTransmittance = bsdf->specularTransmittance;
            //material.specularReflectanceTextureID = bsdf->diffuseReflectanceTexturePath.length() > 0 ? 1 : 0;

            material.eta = bsdf->eta;
            material.k = bsdf->k;

            material.extIOR = bsdf->extIOR;
            material.intIOR = bsdf->intIOR;

            uint microfacetDistribution = 0;
            if (strcmp(bsdf->microfacetDistribution.c_str(), "beckmann") == 0) {
                microfacetDistribution = 0;
            }
            else if (strcmp(bsdf->microfacetDistribution.c_str(), "ggx") == 0) {
                microfacetDistribution = 1;
            }
            else if (strcmp(bsdf->microfacetDistribution.c_str(), "phong") == 0) {
                microfacetDistribution = 2;
            }
            material.microfacetDistribution = microfacetDistribution;
            material.roughness = bsdf->alpha;

            materialData.push_back(material);
        }

        mpMaterialBuffer = createBuffer(mpDevice, sizeof(materialData[0]) * materialData.size(), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* pData;
        d3d_call(mpMaterialBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, materialData.data(), sizeof(materialData[0]) * materialData.size());
        mpMaterialBuffer->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements = materialData.size();
        srvDesc.Buffer.StructureByteStride = sizeof(materialData[0]);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //D3D12_CPU_DESCRIPTOR_HANDLE g_SceneMaterialInfo = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        //g_SceneMaterialInfo.ptr += 2 * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        mpDevice->CreateShaderResourceView(mpMaterialBuffer, &srvDesc, srvHandle);
        srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    }
    
    // 4. Geometry Data
    {
        uint32 indicesSize = 0;
        uint32 verticesSize = 0;
        for (Mesh& mesh : scene->getMeshes()) {
            mesh.indicesOffset = indicesSize;
            mesh.verticesOffset = verticesSize;
            indicesSize += mesh.indicesNumber;
            verticesSize += mesh.verticesNumber;
        }
        scene->indicesNumber = indicesSize;
        scene->verticesNumber = verticesSize;

        std::vector<GeometryInfo> geometryInfoData;
        assert(scene->meshRefID.size() == scene->meshes.size());

        for (int i = 0; i < scene->meshRefID.size(); i++) {
            GeometryInfo geometryInfo;
            Mesh& mesh = scene->getMeshes()[i];
            geometryInfo.materialIndex = scene->materialIDDictionary[scene->meshRefID[i]];
            geometryInfo.lightIndex = mesh.lightIndex;
            geometryInfo.indicesOffset = mesh.indicesOffset;
            geometryInfo.verticesOffset = mesh.verticesOffset;
            //mat3 normalTransform = mat3(transpose(inverse(mesh.transform)));
            //geometryInfo.normalTransform = transpose(normalTransform);
            geometryInfoData.push_back(geometryInfo);
        }

        mpGeometryInfoBuffer = createBuffer(mpDevice, sizeof(geometryInfoData[0]) * geometryInfoData.size(), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* pData2;
        d3d_call(mpGeometryInfoBuffer->Map(0, nullptr, (void**)&pData2));
        memcpy(pData2, geometryInfoData.data(), sizeof(geometryInfoData[0]) * geometryInfoData.size());
        mpGeometryInfoBuffer->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements = geometryInfoData.size();
        srvDesc.Buffer.StructureByteStride = sizeof(geometryInfoData[0]);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //D3D12_CPU_DESCRIPTOR_HANDLE g_SceneMeshInfo = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        //g_SceneMeshInfo.ptr += 3 * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        mpDevice->CreateShaderResourceView(mpGeometryInfoBuffer, &srvDesc, srvHandle);
        srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // 5. Indices Data
    {
        std::vector<uint32> indicesData(scene->indicesNumber);
        uint32 counter = 0;
        for (Mesh& mesh : scene->getMeshes()) {
            for (int i = 0; i < mesh.indicesNumber; i++) {
                indicesData[counter] = (mesh.indices[i]);
                counter++;
            }
        }

        mpIndicesBuffer = createBuffer(mpDevice, sizeof(indicesData[0]) * indicesData.size(), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* pData;
        d3d_call(mpIndicesBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, indicesData.data(), sizeof(indicesData[0]) * indicesData.size());
        mpIndicesBuffer->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R32_UINT;
        srvDesc.Buffer.NumElements = indicesData.size();
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        //descriptorHandle.ptr += 4 * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        mpDevice->CreateShaderResourceView(mpIndicesBuffer, &srvDesc, srvHandle);
        srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // 6. Vertices Data
    {
        std::vector<MeshVertex> verticesData(scene->verticesNumber);
        uint32 counter = 0;
        for (Mesh& mesh : scene->getMeshes()) {
            for (int i = 0; i < mesh.verticesNumber; i++) {
                verticesData[counter].position = mesh.vertices[i];
                verticesData[counter].normal = mesh.normals[i];
                verticesData[counter].uv = mesh.texcoords[i];
                counter++;
            }
        }

        mpVerticesBuffer = createBuffer(mpDevice, sizeof(verticesData[0]) * verticesData.size(), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* pData;
        d3d_call(mpVerticesBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, verticesData.data(), sizeof(verticesData[0]) * verticesData.size());
        mpVerticesBuffer->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements = verticesData.size();
        srvDesc.Buffer.StructureByteStride = sizeof(verticesData[0]);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        //D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        //descriptorHandle.ptr += 5 * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        mpDevice->CreateShaderResourceView(mpVerticesBuffer, &srvDesc, srvHandle);
        srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

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

    createSRVTexture(DXGI_FORMAT_R16_FLOAT, "gHistoryLengthPrev");

    // 15, 16, 17, 18 direct / indirect prev
    createSRVTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, "gDirectIlluminationColorHistory");
    createSRVTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, "gDirectIlluminationMomentHistory");
    createSRVTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, "gIndirectIlluminationColorHistory");
    createSRVTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, "gIndirectIlluminationMomentHistory");

    // HDR
    // createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "HDR", 4);
    
    // Moment
    // createUAVBuffer(DXGI_FORMAT_R16G16_FLOAT, "Moment", 4);

    // Normal
    // createUAVBuffer(DXGI_FORMAT_R8G8B8A8_SNORM, "gOutputNormal", 2);

    // Index
    //createUAVBuffer(DXGI_FORMAT_R32G32B32A32_FLOAT, "gOutputPositionGeomID", 2);
    // Depth
    //createUAVBuffer(DXGI_FORMAT_R32_FLOAT, "gOutputDepth", 2);

    // other
    // createUAVBuffer(DXGI_FORMAT_R32_UINT, "AccumFrameCount");

    textureStartHeapOffset = (srvHandle.ptr - mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr) / mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mpSrvUavHeapCount = textureStartHeapOffset;
}

void TutorialPathTracer::createTextureShaderResources()
{
    mpTextureBuffers.resize(scene->textures.size());
    for (int i = 0; i < scene->textures.size(); i++)
    {
        Texture* texture = scene->textures[i];
        CreateTexture(mpDevice, texture->textureImage->GetMetadata(), &mpTextureBuffers[i]);
        std::vector<D3D12_SUBRESOURCE_DATA> vecSubresources;

        PrepareUpload(mpDevice
            , texture->textureImage->GetImages()
            , texture->textureImage->GetImageCount()
            , texture->textureImage->GetMetadata()
            , vecSubresources);

        // upload is implemented by application developer. Here's one solution using <d3dx12.h>
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mpTextureBuffers[i], 0, static_cast<unsigned int>(vecSubresources.size()));
        Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;
        const D3D12_HEAP_PROPERTIES kHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC tempDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        mpDevice->CreateCommittedResource(
            &kHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &tempDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));

        UpdateSubresources(mpCmdList,
            mpTextureBuffers[i],
            textureUploadHeap.Get(),
            0, 0,
            static_cast<unsigned int>(vecSubresources.size()),
            vecSubresources.data());

        mpCmdList->Close();

        ID3D12CommandList* ppCommandLists[] = { mpCmdList };
        mpCmdQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        mFenceValue++;
        mpCmdQueue->Signal(mpFence, mFenceValue);
        if (mpFence->GetCompletedValue() < mFenceValue) {

            // Fire event when GPU hits current fence.  
            mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);

            // Wait until the GPU hits current fence event is fired.
            WaitForSingleObject(mFenceEvent, INFINITE);
            // CloseHandle(mFenceEvent);
        }

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

        // 4. SRV Descriptor
        // D3D12_CPU_DESCRIPTOR_HANDLE handle = m_pSRV->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        descriptorHandle.ptr += mpSrvUavHeapCount * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        texture->descriptorHandleOffset = mpSrvUavHeapCount;
        mpSrvUavHeapCount++;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texture->textureImage->GetMetadata().format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        mpDevice->CreateShaderResourceView(mpTextureBuffers[i], &srvDesc, descriptorHandle);
    }
}

//RenderTexture *TutorialPathTracer::createRenderTexture(DXGI_FORMAT format)
//{
//    RenderTexture* renderTexture = new RenderTexture();
//
//    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvHeap.pHeap->GetCPUDescriptorHandleForHeapStart();
//    rtvHandle.ptr += mRtvHeap.usedEntries * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
//    mRtvHeap.usedEntries++;
//
//    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
//    srvHandle.ptr += mpSrvUavHeapCount * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//    
//    renderTexture->mpDevice = mpDevice;
//    renderTexture->mRtvDescriptorHandle = rtvHandle;
//    renderTexture->mSrvDescriptorHandle = srvHandle;
//    renderTexture->mpSrvHeap = mpSrvUavHeap;
//    renderTexture->mSrvDescriptorHandleOffset = mpSrvUavHeapCount;
//    renderTexture->createWithSize(mSwapChainSize.x, mSwapChainSize.y, format);
//    mpSrvUavHeapCount++;
//    return renderTexture;
//}


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

    createAccelerationStructures();
    createRtPipelineState();
    createShaderResources();
    createTextureShaderResources();
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
}

//D3D12_GPU_DESCRIPTOR_HANDLE TutorialPathTracer::getGPUHandler(int index)
//{
//    D3D12_GPU_DESCRIPTOR_HANDLE handle = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
//    handle.ptr += index * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//    return handle;
//}
//D3D12_GPU_DESCRIPTOR_HANDLE TutorialPathTracer::getGPUHandlerByName(const char* name)
//{
//    bool notFound = mSrvHeapIndexMap.find(name) == mSrvHeapIndexMap.end();
//    assert(!notFound);
//    int index = this->mSrvHeapIndexMap[name];
//
//    return getGPUHandler(index);
//}

void TutorialPathTracer::onFrameRender()
{
    uint32_t rtvIndex = beginFrame();
    ID3D12ResourcePtr mpOutputResource = outputUAVBuffers["output"];

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
    D3D12_GPU_DESCRIPTOR_HANDLE g_SceneMeshInfo = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    g_SceneMeshInfo.ptr += 2 * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mpCmdList->SetComputeRootDescriptorTable(2, g_SceneMeshInfo);//2 at createGlobalRootDesc

    // UAV
    D3D12_GPU_DESCRIPTOR_HANDLE g_SceneOutput = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    g_SceneOutput.ptr += 6 * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mpCmdList->SetComputeRootDescriptorTable(3, g_SceneOutput);//3 at createGlobalRootDesc


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
    endFrame(rtvIndex);
}

void TutorialPathTracer::initPostProcess()
{
    // Create vertex buffer

    // a triangle
    vec3 vList[] = {
        vec3(1.0f, 1.0f, 0.0f),
        vec3(1.0f, -1.0f, 0.0f),
        vec3(-1.0f, -1.0f, 0.0f),

        vec3(-1.0f, -1.0f, 0.0f),
        vec3(-1.0f, 1.0f, 0.0f),
        vec3(1.0f, 1.0f, 0.0f),
    };

    int vBufferSize = sizeof(vList);

    // create default heap
    // default heap is memory on the GPU. Only the GPU has access to this memory
    // To get data into this heap, we will have to upload the data using
    // an upload heap
    mpDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
                                        // from the upload heap to this heap
        nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
        IID_PPV_ARGS(&vertexBuffer));

    // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
    vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

    // create upload heap
    // upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
    // We will upload the vertex buffer using this heap to the default heap
    ID3D12Resource* vBufferUploadHeap;
    mpDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
        nullptr,
        IID_PPV_ARGS(&vBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(vList); // pointer to our vertex array
    vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
    vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(mpCmdList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

    // transition the vertex buffer data from copy destination state to vertex buffer state
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    // Now we execute the command list to upload the initial assets (triangle data)
    mpCmdList->Close();
    ID3D12CommandList* ppCommandLists[] = { mpCmdList };
    mpCmdQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
    mFenceValue++;
    mpCmdQueue->Signal(mpFence, mFenceValue);

    // create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(vec3);
    vertexBufferView.SizeInBytes = vBufferSize;

    uint Width = this->mSwapChainSize.x;
    uint Height = this->mSwapChainSize.y;

    // Fill out the Viewport
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = Width;
    viewport.Height = Height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Fill out a scissor rect
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = Width;
    scissorRect.bottom = Height;
    
    // this->depthDerivativeShader = new Shader(L"VertexShader.hlsl", L"DepthDerivative.hlsl", mpDevice, 1);

    //this->motionVectorShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFMotionVector.hlsl", mpDevice, 5);
    //this->temporalAccumulationShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFTemporalAccumulation.hlsl", mpDevice, 5);
    //this->waveletShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFATrousWavelet.hlsl", mpDevice, 3);
    //this->reconstructionShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFReconstruction.hlsl", mpDevice, 4);

    this->defaultCopyShader = new Shader(L"QuadVertexShader.hlsl", L"CopyShader.hlsl", mpDevice, 1);
    this->tonemapShader = new Shader(L"QuadVertexShader.hlsl", L"Tonemap.hlsl", mpDevice, 1);

    svgfPass = new SVGFPass(mpDevice, mSwapChainSize);
    svgfPass->createRenderTextures(mRtvHeap.pHeap, mRtvHeap.usedEntries, mpSrvUavHeap, mpSrvUavHeapCount);
}

void TutorialPathTracer::postProcess(int rtvIndex)
{
    const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    
    mpCmdList->RSSetViewports(1, &viewport); // set the viewports
    mpCmdList->RSSetScissorRects(1, &scissorRect); // set the scissor rects
    mpCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology
    mpCmdList->IASetVertexBuffers(0, 1, &vertexBufferView); // set the vertex buffer (using the vertex buffer view)

    // (0) depth derivative!
    //mpCmdList->SetPipelineState(depthDerivativeShader->getPipelineStateObject());
    //mpCmdList->SetGraphicsRootSignature(depthDerivativeShader->getRootSignature()); // set the root signature

    //mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthDerivativeRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    //
    //mpCmdList->OMSetRenderTargets(1, &depthDerivativeRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);

    //mpCmdList->SetGraphicsRootDescriptorTable(1, getGPUHandlerByName("gNormal"));

    //mpCmdList->DrawInstanced(6, 1, 0, 0);
    //mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthDerivativeRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


    svgfPass->forward(mpCmdList, gpuHandlesMap, outputUAVBuffers, mpCameraConstantBuffer);

    // (5) Render to default & tonemap !!
    mpCmdList->SetPipelineState(tonemapShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(tonemapShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &mFrameObjects[rtvIndex].rtvHandle, FALSE, nullptr);
    mpCmdList->ClearRenderTargetView(mFrameObjects[rtvIndex].rtvHandle, clearColor, 0, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, svgfPass->reconstructionRenderTexture->getGPUSrvHandler());
    // mpCmdList->SetGraphicsRootDescriptorTable(1, getGPUHandler(waveletDirect[waveletDirect.size() - 1]->mSrvDescriptorHandleOffset));
    // mpCmdList->SetGraphicsRootDescriptorTable(1, getGPUHandler(temporalAccumulationTextureDirect->mSrvDescriptorHandleOffset));
    // mpCmdList->SetGraphicsRootDescriptorTable(1, getGPUHandlerByName("gDirectIllumination"));

    mpCmdList->DrawInstanced(6, 1, 0, 0);

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
    //Scene* scene = new Scene("cornell-box");
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