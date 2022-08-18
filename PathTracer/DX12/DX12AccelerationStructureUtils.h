#pragma once
#include "DX12BufferUtils.h"
#include "Scene.h"

struct AccelerationStructureBuffers
{
    ID3D12ResourcePtr pScratch;
    ID3D12ResourcePtr pResult;

    // Used only for top-level AS
    ID3D12ResourcePtr pInstanceDesc;
};

AccelerationStructureBuffers createBottomLevelASTriangleMesh(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pVB, uint nVB, ID3D12ResourcePtr pIB, uint nIB);
AccelerationStructureBuffers createTopLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, vector<pair<ID3D12ResourcePtr, mat4>>& bottomLevelASwithTransform, uint64_t& tlasSize);
