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

    // Create SRV.
    mpDevice->CreateShaderResourceView(mResource, nullptr, mSrvDescriptorHandle);

    mWidth = width;
    mHeight = height;
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::getGPUSrvHandler() {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = mpSrvHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += this->mSrvDescriptorHandleOffset * mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return handle;
}

RenderTexture* createRenderTexture(
	ID3D12Device5Ptr pDevice,
	ID3D12DescriptorHeapPtr pRTVHeap,
	uint32_t& usedRTVHeapEntries,
	ID3D12DescriptorHeapPtr pSRVHeap,
	uint32_t& usedSRVHeapEntries,
	uvec2 size,
	DXGI_FORMAT format
)
{
	RenderTexture* renderTexture = new RenderTexture();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += usedRTVHeapEntries * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = pSRVHeap->GetCPUDescriptorHandleForHeapStart();
	srvHandle.ptr += usedSRVHeapEntries * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	renderTexture->mpDevice = pDevice;
	renderTexture->mRtvDescriptorHandle = rtvHandle;
	renderTexture->mSrvDescriptorHandle = srvHandle;
	renderTexture->mSrvDescriptorHandleOffset = usedSRVHeapEntries;
	renderTexture->createWithSize(size.x, size.y, format);
	renderTexture->mpSrvHeap = pSRVHeap;
	usedRTVHeapEntries++;
	usedSRVHeapEntries++;

	return renderTexture;
}