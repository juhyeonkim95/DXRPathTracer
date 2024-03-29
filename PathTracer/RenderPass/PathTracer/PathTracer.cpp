#include "PathTracer.h"
#include <string>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"


PathTracer::PathTracer(ID3D12Device5Ptr mpDevice, Scene* pScene, uvec2 size)
    :RenderPass(mpDevice, size)
{
    this->mpScene = pScene;

    mParam.maxDepthDiffuse = 4;
    mParam.maxDepthSpecular = 4;
    mParam.maxDepthTransmittance = 10;
    mParam.accumulate = true;
    mDefaultParam = mParam;

    ID3DBlobPtr pDxilLib = compileLibrary(kShaderFile, L"", kShaderModel);
    const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kMissEnvShader, kClosestHitShader, kShadowMiss, kShadowChs };
    this->mShader = DxilLibrary(pDxilLib, entryPoints, arraysize(entryPoints));

    this->createRtPipelineState();
}

void PathTracer::createRtPipelineState()
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
    subobjects[index++] = this->mShader.stateSubobject; // 0 Library

    HitProgram hitProgram(nullptr, kClosestHitShader, kHitGroup);
    subobjects[index++] = hitProgram.subObject; // 1 Hit Group

    // Create the shadow-ray hit group
    HitProgram shadowHitProgram(nullptr, kShadowChs, kShadowHitGroup);
    subobjects[index++] = shadowHitProgram.subObject; // 2 Shadow Hit Group

    D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
    emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    // Create the ray-gen root-signature and association
    LocalRootSignature rgsRootSignature(mpDevice, emptyDesc);// createRayGenRootDesc().desc);
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
    LocalRootSignature missRootSignature(mpDevice, emptyDesc);
    subobjects[index] = missRootSignature.subobject; // 7 Miss Root Sig

    uint32_t emptyRootIndex = index++; // 7
    const WCHAR* emptyRootExport[] = { kMissShader, kMissEnvShader , kShadowChs, kShadowMiss };
    ExportAssociation missRootAssociation(emptyRootExport, arraysize(emptyRootExport), &(subobjects[emptyRootIndex]));
    subobjects[index++] = missRootAssociation.subobject; // 8 Associate Miss Root Sig to Miss Shader

    // Bind the payload size to the programs
    ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 50);
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
    mpGlobalRootSig = root.pRootSig;
    subobjects[index++] = root.subobject; // 12

    // Create the state
    D3D12_STATE_OBJECT_DESC desc;
    desc.NumSubobjects = index; // 13
    desc.pSubobjects = subobjects.data();
    desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

    d3d_call(mpDevice->CreateStateObject(&desc, IID_PPV_ARGS(&mpPipelineState)));
}

RootSignatureDesc PathTracer::createRayGenRootDesc()
{
    // Create the root-signature
    RootSignatureDesc desc;
    desc.range.resize(1);

    // gRtScene
    desc.range[0].BaseShaderRegister = 0;
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    // desc.range[0].OffsetInDescriptorsFromTableStart = 0;

    desc.rootParams.resize(1);
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[0].DescriptorTable.pDescriptorRanges = desc.range.data();

    // Create the desc
    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}

RootSignatureDesc PathTracer::createHitRootDesc()
{
    RootSignatureDesc desc;

    desc.range.resize(1);

    // SRV for texture
    desc.range[0].BaseShaderRegister = 6;
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;

    desc.rootParams.resize(1);

    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[0].DescriptorTable.pDescriptorRanges = &desc.range[0];

    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}

RootSignatureDesc PathTracer::createGlobalRootDesc()
{
    RootSignatureDesc desc;

    int nSwap = 10;
    desc.range.resize(2 + nSwap);

    // SRV
    // (0) Acceleration Structure (Structured Buffer)
    // (1) material Info (Structured Buffer)
    // (2) geometry Info (Structured Buffer)
    // (3) indices Info (uint Buffer)
    // (4) vertices Info (Structured Buffer)
    // (5) Envmap ()
    desc.range[0].BaseShaderRegister = 0;
    desc.range[0].NumDescriptors = 6;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    // UAV
    desc.range[1].BaseShaderRegister = 0;
    desc.range[1].NumDescriptors = 20;
    desc.range[1].RegisterSpace = 0;
    desc.range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

    for (int i = 0; i < nSwap; i++)
    {
        desc.range[i + 2].BaseShaderRegister = 20 + i;
        desc.range[i + 2].NumDescriptors = 1;
        desc.range[i + 2].RegisterSpace = 0;
        desc.range[i + 2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    }

    desc.rootParams.resize(5 + nSwap);

    // camera 
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    desc.rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    desc.rootParams[0].Descriptor.RegisterSpace = 0;
    desc.rootParams[0].Descriptor.ShaderRegister = 0;

    // material
    desc.rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    desc.rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    desc.rootParams[1].Descriptor.RegisterSpace = 0;
    desc.rootParams[1].Descriptor.ShaderRegister = 1;

    // mParams
    desc.rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    desc.rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    desc.rootParams[2].Descriptor.RegisterSpace = 0;
    desc.rootParams[2].Descriptor.ShaderRegister = 2;

    // srv
    desc.rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[3].DescriptorTable.pDescriptorRanges = &desc.range[0];

    // uav
    desc.rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[4].DescriptorTable.pDescriptorRanges = &desc.range[1];

    // uavs for swap
    for (int i = 0; i < nSwap; i++)
    {
        desc.rootParams[5 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        desc.rootParams[5 + i].DescriptorTable.NumDescriptorRanges = 1;
        desc.rootParams[5 + i].DescriptorTable.pDescriptorRanges = &desc.range[2 + i];
    }


    CD3DX12_STATIC_SAMPLER_DESC bilinearClamp(
        0,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,
        4
    );

    desc.desc.NumParameters = desc.rootParams.size();
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    desc.desc.NumStaticSamplers = 1;
    desc.desc.pStaticSamplers = &bilinearClamp;

    return desc;
}

void PathTracer::createShaderTable(HeapData* pSrvUavHeap)
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
    uint32_t nMeshes = this->mpScene->getMeshes().size();
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
    uint64_t heapStart = pSrvUavHeap->getGPUHandleByName("AccelerationStructure").ptr;// ->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart().ptr;
    *(uint64_t*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart;

    // Entry 1 - miss program
    if (mpScene->envMapTexture) {
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

        Mesh& mesh = mpScene->meshes[i];
        std::string meshRefID = mpScene->meshRefID[i];
        int bsdfID = mpScene->materialIDDictionary[meshRefID];
        BSDF* meshBSDF = mpScene->bsdfs[bsdfID];

        if (meshBSDF->diffuseReflectanceTexturePath.length() > 0) {
            int textureID = mpScene->textureIDDictionary[meshBSDF->diffuseReflectanceTexturePath];
            Texture* texture = mpScene->textures[textureID];
            *(uint64_t*)(pCbDesc) = pSrvUavHeap->getGPUHandle(texture->descriptorHandleOffset).ptr;
        }

        // Closest hit for shadow ray
        uint8_t* pEntryShadow = pData + mShaderTableEntrySize * (2 * i + 4);
        memcpy(pEntryShadow, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }

    // Unmap
    mpShaderTable->Unmap(0, nullptr);
}

void PathTracer::createShaderResources(HeapData *pSrvUavHeap)
{
    // LDR / HDR
    mUAVResourceDictionary["gOutput"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_UNORM, "gOutput", 1);
    mUAVResourceDictionary["gOutputHDR"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gOutputHDR", 1);

    // Direct & indirect
    mUAVResourceDictionary["gDirectIllumination"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDirectIllumination", 1);
    mUAVResourceDictionary["gIndirectIllumination"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gIndirectIllumination", 1);

    // Diffuse & specular
    mUAVResourceDictionary["gDiffuseRadiance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDiffuseRadiance", 1);
    mUAVResourceDictionary["gSpecularRadiance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gSpecularRadiance", 1);

    mUAVResourceDictionary["gEmission"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gEmission", 1);

    // Reflectance
    mUAVResourceDictionary["gReflectance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_UNORM, "gReflectance", 1);
    mUAVResourceDictionary["gDiffuseReflectance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_UNORM, "gDiffuseReflectance", 1);
    mUAVResourceDictionary["gSpecularReflectance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_UNORM, "gSpecularReflectance", 1);

    // Delta Reflection / Transmission
    mUAVResourceDictionary["gDeltaReflectionReflectance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_UNORM, "gDeltaReflectionReflectance", 1);
    mUAVResourceDictionary["gDeltaReflectionEmission"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaReflectionEmission", 1);
    mUAVResourceDictionary["gDeltaReflectionRadiance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaReflectionRadiance", 1);

    mUAVResourceDictionary["gDeltaTransmissionReflectance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_UNORM, "gDeltaTransmissionReflectance", 1);
    mUAVResourceDictionary["gDeltaTransmissionEmission"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaTransmissionEmission", 1);
    mUAVResourceDictionary["gDeltaTransmissionRadiance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaTransmissionRadiance", 1);

    // Others
    mUAVResourceDictionary["gResidualRadiance"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gResidualRadiance", 1);
    mUAVResourceDictionary["gPathType"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32_UINT, "gPathType", 1);
    mUAVResourceDictionary["gRoughness"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32_FLOAT, "gRoughness", 1);
    mUAVResourceDictionary["gMotionVector"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32_FLOAT, "gMotionVector", 1);


    // ***************
    // To be swapped
    // ***************
    
    // PositionID / NormalDepth
    mUAVResourceDictionary["gPositionMeshID"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gPositionMeshID", 1);
    mUAVResourceDictionary["gPositionMeshIDPrev"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gPositionMeshIDPrev", 1);

    mUAVResourceDictionary["gNormalDepth"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gNormalDepth", 1);
    mUAVResourceDictionary["gNormalDepthPrev"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gNormalDepthPrev", 1);

    mUAVResourceDictionary["gDeltaReflectionPositionMeshID"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaReflectionPositionMeshID", 1);
    mUAVResourceDictionary["gDeltaReflectionNormal"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_SNORM, "gDeltaReflectionNormal", 1);
    mUAVResourceDictionary["gDeltaTransmissionPositionMeshID"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaTransmissionPositionMeshID", 1);
    mUAVResourceDictionary["gDeltaTransmissionNormal"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_SNORM, "gDeltaTransmissionNormal", 1);

    // Reservoirs for ReSTIR
    mUAVResourceDictionary["gPrevReserviors"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_UNKNOWN, "gPrevReserviors", 1, sizeof(Reservoir));
    mUAVResourceDictionary["gCurrReserviors"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_UNKNOWN, "gCurrReserviors", 1, sizeof(Reservoir));


    // not visible by shader
    mUAVResourceDictionary["gDeltaReflectionPositionMeshIDPrev"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaReflectionPositionMeshIDPrev", 1);
    mUAVResourceDictionary["gDeltaTransmissionPositionMeshIDPrev"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT, "gDeltaTransmissionPositionMeshIDPrev", 1);
    mUAVResourceDictionary["gDeltaReflectionNormalPrev"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_SNORM, "gDeltaReflectionNormalPrev", 1);
    mUAVResourceDictionary["gDeltaTransmissionNormalPrev"] = createUAVBuffer(mpDevice, pSrvUavHeap, size, DXGI_FORMAT_R8G8B8A8_SNORM, "gDeltaTransmissionNormalPrev", 1);
}

void PathTracer::processGUI()
{
    mDirty = false;
    if (ImGui::CollapsingHeader("PathTracer"))
    {
        mDirty |= ImGui::SliderInt("Max Depth - diffuse", &mParam.maxDepthDiffuse, 1, 16);
        mDirty |= ImGui::SliderInt("Max Depth - specular", &mParam.maxDepthSpecular, 1, 16);
        mDirty |= ImGui::SliderInt("Max Depth - transmittance", &mParam.maxDepthTransmittance, 1, 16);
        mDirty |= ImGui::Checkbox("Accumulate", &mParam.accumulate);
    }
}

void PathTracer::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr pCmdList = pRenderContext->pCmdList;
    SceneResourceManager* pSceneResourceManager = pRenderContext->pSceneResourceManager;
    HeapData* pSrvUavHeap = pRenderContext->pSrvUavHeap;
    ReSTIRParameters *restirParam = pRenderContext->restirParam;

    // Prepare resource barrier
    for (auto it = mUAVResourceDictionary.begin(); it != mUAVResourceDictionary.end(); ++it)
    {
        resourceBarrier(pCmdList, it->second, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }


    D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
    raytraceDesc.Width = size.x;
    raytraceDesc.Height = size.y;
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
    raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize * (2 * mpScene->getMeshes().size());

    // Bind the root signature
    pCmdList->SetComputeRootSignature(mpGlobalRootSig);
    pCmdList->SetComputeRootConstantBufferView(0, pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());
    pCmdList->SetComputeRootConstantBufferView(1, pSceneResourceManager->getLightConstantBuffer()->GetGPUVirtualAddress());
    
    if (!mpParamBuffer)
    {
        mpParamBuffer = createBuffer(mpDevice, sizeof(ReSTIRParameters) + sizeof(PathTracerParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    }

    uint8_t* pData;
    int offset = 0;
    d3d_call(mpParamBuffer->Map(0, nullptr, (void**)&pData));

    memcpy(pData, &this->mParam, sizeof(PathTracerParameters));
    offset += sizeof(PathTracerParameters);

    memcpy(pData + offset, restirParam, sizeof(ReSTIRParameters));
    offset += sizeof(ReSTIRParameters);

    mpParamBuffer->Unmap(0, nullptr);
    pCmdList->SetComputeRootConstantBufferView(2, mpParamBuffer->GetGPUVirtualAddress());


    // SRVs
    pCmdList->SetComputeRootDescriptorTable(3, pSceneResourceManager->getSRVStartHandle());//3 at createGlobalRootDesc

    // UAVs
    pCmdList->SetComputeRootDescriptorTable(4, pSrvUavHeap->getGPUHandleByName("gOutput"));//4 at createGlobalRootDesc
    
    pCmdList->SetComputeRootDescriptorTable(5, pSrvUavHeap->getGPUHandleByName("gPositionMeshID"));
    pCmdList->SetComputeRootDescriptorTable(6, pSrvUavHeap->getGPUHandleByName("gPositionMeshIDPrev"));

    pCmdList->SetComputeRootDescriptorTable(7, pSrvUavHeap->getGPUHandleByName("gNormalDepth"));
    pCmdList->SetComputeRootDescriptorTable(8, pSrvUavHeap->getGPUHandleByName("gNormalDepthPrev"));

    pCmdList->SetComputeRootDescriptorTable(9, pSrvUavHeap->getGPUHandleByName("gDeltaReflectionPositionMeshID"));
    pCmdList->SetComputeRootDescriptorTable(10, pSrvUavHeap->getGPUHandleByName("gDeltaReflectionNormal"));
    pCmdList->SetComputeRootDescriptorTable(11, pSrvUavHeap->getGPUHandleByName("gDeltaTransmissionPositionMeshID"));
    pCmdList->SetComputeRootDescriptorTable(12, pSrvUavHeap->getGPUHandleByName("gDeltaTransmissionNormal"));
    
    pCmdList->SetComputeRootDescriptorTable(13, pSrvUavHeap->getGPUHandleByName("gPrevReserviors"));
    pCmdList->SetComputeRootDescriptorTable(14, pSrvUavHeap->getGPUHandleByName("gCurrReserviors"));


    // Dispatch
    pCmdList->SetPipelineState1(mpPipelineState.GetInterfacePtr());
    pCmdList->DispatchRays(&raytraceDesc);

    // end resource barrier
    for (auto it = mUAVResourceDictionary.begin(); it != mUAVResourceDictionary.end(); ++it)
    {
        resourceBarrier(pCmdList, it->second, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    }

    renderData.addOutputs(pSrvUavHeap->getGPUHandleMap());

    swapHandle(pSrvUavHeap, "gPositionMeshID", "gPositionMeshIDPrev");
    swapHandle(pSrvUavHeap, "gNormalDepth", "gNormalDepthPrev");
    swapHandle(pSrvUavHeap, "gDeltaReflectionPositionMeshID", "gDeltaReflectionPositionMeshIDPrev");
    swapHandle(pSrvUavHeap, "gDeltaReflectionNormal", "gDeltaReflectionNormalPrev");
    swapHandle(pSrvUavHeap, "gDeltaTransmissionPositionMeshID", "gDeltaTransmissionPositionMeshIDPrev");
    swapHandle(pSrvUavHeap, "gDeltaTransmissionNormal", "gDeltaTransmissionNormalPrev");
    swapHandle(pSrvUavHeap, "gPrevReserviors", "gCurrReserviors");
}

void PathTracer::swapHandle(HeapData* pSrvUavHeap, const char* name1, const char* name2)
{
    std::swap(this->mUAVResourceDictionary.at(name1), this->mUAVResourceDictionary.at(name2));
    pSrvUavHeap->swapHandle(name1, name2);
}