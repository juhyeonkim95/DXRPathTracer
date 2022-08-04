#include "SceneResourceManager.h"
#include "DirectXTex.h"
#include "DirectXTex.inl"
#include "d3dx12.h"
#include "DX12Utils.h"

SceneResourceManager::SceneResourceManager(Scene* scene, ID3D12Device5Ptr pDevice, HeapData* srvHeap)
{
    this->scene = scene;
    this->mpDevice = pDevice;
    this->mSrvUavHeap = srvHeap;
    this->mpSceneAccelerationStructure = new SceneAccelerationStructure(scene);
}

void SceneResourceManager::createSceneCBVs()
{
    mpLightParametersBuffer = createBuffer(mpDevice, sizeof(LightParameter) * 20, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    d3d_call(mpLightParametersBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, scene->lights.data(), sizeof(LightParameter) * scene->lights.size());
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

        mpDevice->CreateShaderResourceView(mpMaterialBuffer, &srvDesc, mSrvUavHeap->addDescriptorHandle("MaterialData"));

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

        mpDevice->CreateShaderResourceView(mpGeometryInfoBuffer, &srvDesc, mSrvUavHeap->addDescriptorHandle("GeometryData"));
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

        mpDevice->CreateShaderResourceView(mpIndicesBuffer, &srvDesc, mSrvUavHeap->addDescriptorHandle("IndicesData"));
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
        mpDevice->CreateShaderResourceView(mpVerticesBuffer, &srvDesc, mSrvUavHeap->addDescriptorHandle("VerticesData"));
    }
}

void SceneResourceManager::createSceneAccelerationStructure(ID3D12CommandQueuePtr mpCmdQueue, ID3D12CommandAllocatorPtr pCmdAllocator, ID3D12GraphicsCommandList4Ptr mpCmdList, ID3D12FencePtr mpFence, HANDLE mFenceEvent, uint64_t& mFenceValue)
{
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
        ID3D12CommandList* pGraphicsList = mpCmdList.GetInterfacePtr();
        mpCmdQueue->ExecuteCommandLists(1, &pGraphicsList);
        mFenceValue++;
        mpCmdQueue->Signal(mpFence, mFenceValue);
        mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
        mpCmdList->Reset(pCmdAllocator, nullptr);

        // 4. SRV Descriptor
        // D3D12_CPU_DESCRIPTOR_HANDLE handle = m_pSRV->GetCPUDescriptorHandleForHeapStart();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texture->textureImage->GetMetadata().format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        texture->descriptorHandleOffset = mSrvUavHeap->getLastIndex();
        mpDevice->CreateShaderResourceView(mpTextureBuffers[i], &srvDesc, mSrvUavHeap->addDescriptorHandle(texture->name.c_str()));
    }
}
