#include "RenderContext.h"
#include "DX12Helper.h"

RenderContext::RenderContext(ID3D12Device5Ptr pDevice, HeapData* pSrvUavHeap)
{
	this->mpDevice = pDevice;
    this->mpSrvUavHeap = pSrvUavHeap;
}

void RenderContext::createUAVBuffer(DXGI_FORMAT format, uvec2 size, std::string name, uint depth, uint structSize)
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
    d3d_call(mpDevice->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&outputResources))); // Starting as copy-source to simplify onFrameRender()

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

        mpDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, mpSrvUavHeap->addDescriptorHandle(name.c_str()));
    }
    else {
        for (int i = 0; i < depth; i++) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Format = format;
            uavDesc.Texture2DArray.ArraySize = 1;
            uavDesc.Texture2DArray.FirstArraySlice = UINT(i);
            uavDesc.Texture2DArray.PlaneSlice = 0;
            mpDevice->CreateUnorderedAccessView(outputResources, nullptr, &uavDesc, mpSrvUavHeap->addDescriptorHandle(name.c_str()));
        }
    }
    outputUavBuffers[name] = outputResources;
}