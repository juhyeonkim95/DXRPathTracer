#include "DX12BufferUtils.h"
#include "Framework.h"
#include "HeapData.h"

ID3D12ResourcePtr createBuffer(ID3D12Device5Ptr pDevice, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = flags;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = size;

    ID3D12ResourcePtr pBuffer;
    d3d_call(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer)));
    return pBuffer;
}

ID3D12ResourcePtr createUAVBuffer(ID3D12Device5Ptr pDevice, HeapData* pSrvUavHeap, uvec2 size, DXGI_FORMAT format, std::string name, uint depth, uint structSize)
{
    ID3D12ResourcePtr outputResources;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.DepthOrArraySize = depth;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Format = format;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (structSize == 0) {
        resDesc.Width = size.x;
        resDesc.Height = size.y;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    }
    else {
        resDesc.Alignment = 0;
        resDesc.DepthOrArraySize = 1;
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resDesc.Height = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.SampleDesc.Quality = 0;
        resDesc.Width = structSize * size.x * size.y;
    }
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    d3d_call(pDevice->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&outputResources))); // Starting as copy-source to simplify onFrameRender()

    if (depth == 1) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

        if (structSize == 0) {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        }
        else {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.Buffer.StructureByteStride = structSize;
            uavDesc.Buffer.NumElements = size.x * size.y;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        }

        pDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, pSrvUavHeap->addDescriptorHandle(name.c_str()));
    }
    else {
        for (int i = 0; i < depth; i++) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Format = format;
            uavDesc.Texture2DArray.ArraySize = 1;
            uavDesc.Texture2DArray.FirstArraySlice = UINT(i);
            uavDesc.Texture2DArray.PlaneSlice = 0;
            pDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, pSrvUavHeap->addDescriptorHandle(name.c_str()));
        }
    }

    return outputResources;
}