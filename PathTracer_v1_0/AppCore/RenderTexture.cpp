#include "RenderTexture.h"
#include "d3dx12.h"


void RenderTexture::createWithSize(size_t width, size_t height, DXGI_FORMAT format)
{
	auto const heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format,
        static_cast<UINT64>(width),
        static_cast<UINT>(height),
        1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    mState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Create a render target
    mpDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
        &desc, mState, nullptr, IID_PPV_ARGS(&mResource));

    D3D12_RENDER_TARGET_VIEW_DESC rtvdesc = {};
    rtvdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvdesc.Format = format;
    rtvdesc.Texture2D.MipSlice = 0;

    // Create RTV.
    mpDevice->CreateRenderTargetView(mResource, &rtvdesc, mRtvDescriptorHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    // Create SRV.
    mpDevice->CreateShaderResourceView(mResource, &srvDesc, mSrvDescriptorHandle);

    mWidth = width;
    mHeight = height;
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::getGPUSrvHandler() {
    return mSrvDescriptorHandleGPU;
}

RenderTexture* createRenderTexture(
	ID3D12Device5Ptr pDevice,
    HeapData* rtvHeap,
    HeapData* srvHeap,
	uvec2 size,
	DXGI_FORMAT format
)
{
	RenderTexture* renderTexture = new RenderTexture();

	renderTexture->mpDevice = pDevice;
    renderTexture->mSrvDescriptorHandleGPU = srvHeap->getLastGPUHandle();
	renderTexture->mRtvDescriptorHandle = rtvHeap->addDescriptorHandle();
	renderTexture->mSrvDescriptorHandle = srvHeap->addDescriptorHandle();
	renderTexture->createWithSize(size.x, size.y, format);

	return renderTexture;
}