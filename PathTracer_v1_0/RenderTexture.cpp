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

    // Create RTV.
    mpDevice->CreateRenderTargetView(mResource, nullptr, mRtvDescriptorHandle);

    // Create SRV.
    mpDevice->CreateShaderResourceView(mResource, nullptr, mSrvDescriptorHandle);

    mWidth = width;
    mHeight = height;
}