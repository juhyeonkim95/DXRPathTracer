#include "SceneResourceManager.h"
#include "DirectXTex.h"
#include "DirectXTex.inl"
#include "d3dx12.h"
#include "DX12Utils.h"

SceneResourceManager::SceneResourceManager(Scene* pScene, ID3D12Device5Ptr pDevice, HeapData* srvHeap)
{
    this->mpScene = pScene;
    this->mpDevice = pDevice;
    this->mpSrvUavHeap = srvHeap;
    this->mpSceneAccelerationStructure = new SceneAccelerationStructure(pScene);
}

void SceneResourceManager::createSceneCBVs()
{
    mpLightParametersBuffer = createBuffer(mpDevice, sizeof(LightParameter) * 20, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    d3d_call(mpLightParametersBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, mpScene->lights.data(), sizeof(LightParameter) * mpScene->lights.size());
    mpLightParametersBuffer->Unmap(0, nullptr);

    mpCameraConstantBuffer = createBuffer(mpDevice, sizeof(PerFrameData), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
}

void SceneResourceManager::update(PerFrameData&frameData)
{
    int8_t* pData;
    d3d_call(mpCameraConstantBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &frameData, sizeof(PerFrameData));
    mpCameraConstantBuffer->Unmap(0, nullptr);
}


void SceneResourceManager::createSceneSRVs()
{
    // 0. Acceleration Structure
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = getSceneAS()->getTopLevelAS()->GetGPUVirtualAddress();
    mpDevice->CreateShaderResourceView(nullptr, &srvDesc, mpSrvUavHeap->addDescriptorHandle("AccelerationStructure"));

    // 1. Material Data (StructuredBuffer <Material>)
    {
        std::vector<Material> materialData;
        for (BSDF* bsdf : mpScene->bsdfs)
        {
            Material material;
            material.materialType = bsdf->bsdfType;
            material.isDoubleSided = bsdf->isDoubleSided;

            // TODO : ADD specular reflectance / transmittance texture?
            material.diffuseReflectance = bsdf->diffuseReflectance;
            material.diffuseReflectanceTextureID = bsdf->diffuseReflectanceTexturePath.length() > 0 ? 1 : 0;
            material.specularReflectance = bsdf->specularReflectance;
            //material.specularReflectanceTextureID = bsdf->diffuseReflectanceTexturePath.length() > 0 ? 1 : 0;
            material.specularTransmittance = bsdf->specularTransmittance;
            //material.specularReflectanceTextureID = bsdf->diffuseReflectanceTexturePath.length() > 0 ? 1 : 0;
            
            material.conductorReflectance = bsdf->conductorReflectance;
            material.diffuseFresnel = bsdf->diffuseFresnel;

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

        mpDevice->CreateShaderResourceView(mpMaterialBuffer, &srvDesc, mpSrvUavHeap->addDescriptorHandle("MaterialData"));
    }

    // 2. Geometry Data (StructuredBuffer <GeometryInfo>)
    {
        uint32 indicesSize = 0;
        uint32 verticesSize = 0;
        for (Mesh& mesh : mpScene->getMeshes()) {
            mesh.indicesOffset = indicesSize;
            mesh.verticesOffset = verticesSize;
            indicesSize += mesh.indicesNumber;
            verticesSize += mesh.verticesNumber;
        }
        mpScene->indicesNumber = indicesSize;
        mpScene->verticesNumber = verticesSize;

        std::vector<GeometryInfo> geometryInfoData;
        assert(mpScene->meshRefID.size() == mpScene->meshes.size());

        for (int i = 0; i < mpScene->meshRefID.size(); i++) {
            GeometryInfo geometryInfo;
            Mesh& mesh = mpScene->getMeshes()[i];
            geometryInfo.materialIndex = mpScene->materialIDDictionary[mpScene->meshRefID[i]];
            geometryInfo.lightIndex = mesh.lightIndex;
            geometryInfo.indicesOffset = mesh.indicesOffset;
            geometryInfo.verticesOffset = mesh.verticesOffset;
            geometryInfoData.push_back(geometryInfo);
        }

        mpGeometryInfoBuffer = createBuffer(mpDevice, sizeof(geometryInfoData[0]) * geometryInfoData.size(), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* pData;
        d3d_call(mpGeometryInfoBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, geometryInfoData.data(), sizeof(geometryInfoData[0]) * geometryInfoData.size());
        mpGeometryInfoBuffer->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements = geometryInfoData.size();
        srvDesc.Buffer.StructureByteStride = sizeof(geometryInfoData[0]);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        mpDevice->CreateShaderResourceView(mpGeometryInfoBuffer, &srvDesc, mpSrvUavHeap->addDescriptorHandle("GeometryData"));
    }

    // 3. Indices Data (StructuredBuffer <uint>)
    {
        std::vector<uint32> indicesData(mpScene->indicesNumber);
        uint32 counter = 0;
        for (Mesh& mesh : mpScene->getMeshes()) {
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

        mpDevice->CreateShaderResourceView(mpIndicesBuffer, &srvDesc, mpSrvUavHeap->addDescriptorHandle("IndicesData"));
    }

    // 4. Vertices Data (StructuredBuffer <MeshVertex>)
    {
        std::vector<MeshVertex> verticesData(mpScene->verticesNumber);
        uint32 counter = 0;
        for (Mesh& mesh : mpScene->getMeshes()) {
            for (int i = 0; i < mesh.verticesNumber; i++) {
                // verticesData[counter].position = mesh.vertices[i];
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

        mpDevice->CreateShaderResourceView(mpVerticesBuffer, &srvDesc, mpSrvUavHeap->addDescriptorHandle("VerticesData"));
    }
}

void SceneResourceManager::createSceneAccelerationStructure(ID3D12CommandQueuePtr mpCmdQueue, ID3D12CommandAllocatorPtr pCmdAllocator, ID3D12GraphicsCommandList4Ptr mpCmdList, ID3D12FencePtr mpFence, HANDLE mFenceEvent, uint64_t& mFenceValue)
{
    // Commit AS creation to SceneAccelerationStructure
    this->mpSceneAccelerationStructure->createSceneAccelerationStructure(mpDevice, mpCmdQueue, pCmdAllocator, mpCmdList, mpFence, mFenceEvent, mFenceValue);
}

void SceneResourceManager::createSceneTextureResources(
    ID3D12CommandQueuePtr mpCmdQueue,
    ID3D12CommandAllocatorPtr pCmdAllocator,
    ID3D12GraphicsCommandList4Ptr mpCmdList,
    ID3D12FencePtr mpFence,
    HANDLE mFenceEvent,
    uint64_t &mFenceValue
)
{
    // Create Textures used in the mpScene
    // Allocate SRV to each texture.
    // TODO : using TextureArray2D of Array of TextureArray
    // IMPORTANT : if mpScene has a environment light, it always locate at the first position.

    mpTextureBuffers.resize(mpScene->textures.size());
    for (int i = 0; i < mpScene->textures.size(); i++)
    {
        Texture* texture = mpScene->textures[i];
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
        ID3D12CommandList* pGraphicsList = mpCmdList.GetInterfacePtr();
        mpCmdQueue->ExecuteCommandLists(1, &pGraphicsList);
        mFenceValue++;
        mpCmdQueue->Signal(mpFence, mFenceValue);
        mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
        mpCmdList->Reset(pCmdAllocator, nullptr);

        // SRV Descriptor
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texture->textureImage->GetMetadata().format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        texture->descriptorHandleOffset = mpSrvUavHeap->getLastIndex();
        mpDevice->CreateShaderResourceView(mpTextureBuffers[i], &srvDesc, mpSrvUavHeap->addDescriptorHandle(texture->name.c_str()));
    }
}
