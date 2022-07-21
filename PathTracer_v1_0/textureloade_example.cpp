#include <windows.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <vector>
#include <array>
#include <wrl.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <memory>
#include <d3dcompiler.h>
#include <vector>
#include <utility>
#include <cmath>
#include<string>

#include "DirectXTex.h"
#include "DirectXTex.inl"


#define SWAP_CHAIN_BUFFER_COUNT 2
#define WIDTH 800
#define HEIGHT 800
#define IDC_CLIENT 109

#define MaxLights 16

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

#ifdef _DEBUG
#pragma comment(lib, "DirectXTex_debug.lib")
#else
#pragma comment(lib, "DirectXTex.lib")
#endif

struct Vertex {
public:
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;

    Vertex() {}

    Vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v) : Pos(px, py, pz), Normal(nx, ny, nz), TexC(u, v) {}
    Vertex(DirectX::XMFLOAT3 Pos_, DirectX::XMFLOAT3 Normal_, DirectX::XMFLOAT2 TexC_) : Pos(Pos_), Normal(Normal_), TexC(TexC_) {}
};



struct Light {
public:
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot light only
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
    float FalloffEnd = 10.0f;                           // point/spot light only
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
    float SpotPower = 64.0f;                            // spot light only
};

struct ObjectConstants {
public:
    DirectX::XMFLOAT4X4 gWorld;
};


struct MaterialConstants
{
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    // Used in texture mapping.
    DirectX::XMFLOAT4X4 MatTransform;

    MaterialConstants() {
        DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        FresnelR0 = { 0.01f, 0.01f, 0.01f };
        Roughness = 0.25f;

        DirectX::XMMATRIX mat(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        DirectX::XMStoreFloat4x4(&MatTransform, DirectX::XMMatrixTranspose(mat));
    }
};


struct PassConstants {
public:
    DirectX::XMFLOAT4X4 gView, gViewProj;
    Light gLights[MaxLights];
    DirectX::XMFLOAT4 AmbientLight;
    DirectX::XMFLOAT3 EyePosW;
};


DirectX::XMFLOAT2 mousePos;

HWND hWnd;
HACCEL hAccelTable;
MSG msg;
WNDCLASS WndClass;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE g_hInst;
LPCTSTR lpszClass = TEXT("First");

Microsoft::WRL::ComPtr<IDXGIFactory> mdxgiFactory;
Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;
Microsoft::WRL::ComPtr<ID3D12Fence>  mFence;

UINT mRtvDescriptorSize = 0;
UINT mCbvSrvUavDescriptorSize;

Microsoft::WRL::ComPtr<ID3D12CommandQueue>          mCommandQueue;
Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      mDirectCmdListAlloc;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   mCommandList;

Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
const int SwapChainBufferCount = 2;

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap;

UINT64 mCurrentFence = 0;

Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

int mCurrBackBuffer = 0;

D3D12_VIEWPORT mScreenViewport;
D3D12_RECT mScissorRect;

Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;



UINT vbByteSize, ibByteSize;
std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

Microsoft::WRL::ComPtr<ID3D12Resource> mObjectCB = nullptr;
UINT objectCBByteSize = (sizeof(ObjectConstants) + 255) & ~255;

Microsoft::WRL::ComPtr<ID3D12Resource> mMaterialCB = nullptr;
UINT materialCBByteSize = (sizeof(MaterialConstants) + 255) & ~255;

Microsoft::WRL::ComPtr<ID3D12Resource> mPassCB = nullptr;
UINT passCBByteSize = (sizeof(PassConstants) + 255) & ~255;

float timeValue = 0;

Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;

int width, height;

float mPhi = DirectX::XM_PI / 3.0;
float mTheta = DirectX::XM_PI / 4.0;
float mRadius = 7;


Microsoft::WRL::ComPtr<ID3D12Resource>         m_pTex2D;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pSRV;
DirectX::ScratchImage                  m_Image;

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSamplerDescriptorHeap;


void FlushCommandQueue()
{
    // Advance the fence value to mark commands up to this fence point.
    mCurrentFence++;

    // Add an instruction to the command queue to set a new fence point.  Because we 
    // are on the GPU timeline, the new fence point won't be set until the GPU finishes
    // processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);

    // Wait until the GPU has completed commands up to this fence point.
    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.  
        mFence->SetEventOnCompletion(mCurrentFence, eventHandle);

        // Wait until the GPU hits current fence event is fired.
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}



void LoadTexture(const std::wstring& _strFullPath) {
    // 1. load imanage
    HRESULT hr = DirectX::LoadFromWICFile(_strFullPath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, nullptr, m_Image);
    hr = CreateTexture(md3dDevice.Get(), m_Image.GetMetadata(), &m_pTex2D);

    std::vector<D3D12_SUBRESOURCE_DATA> vecSubresources;

    hr = PrepareUpload(md3dDevice.Get()
        , m_Image.GetImages()
        , m_Image.GetImageCount()
        , m_Image.GetMetadata()
        , vecSubresources);

    if (FAILED(hr))
        assert(nullptr);

    // upload is implemented by application developer. Here's one solution using <d3dx12.h>
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pTex2D.Get(), 0, static_cast<unsigned int>(vecSubresources.size()));

    Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;
    hr = md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));

    if (FAILED(hr))
        assert(nullptr);

    UpdateSubresources(mCommandList.Get(),
        m_pTex2D.Get(),
        textureUploadHeap.Get(),
        0, 0,
        static_cast<unsigned int>(vecSubresources.size()),
        vecSubresources.data());

    mCommandList->Close();

    ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    FlushCommandQueue();

    mDirectCmdListAlloc->Reset();
    mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);


    // 3. SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_pSRV));

    // 4. SRV Descriptor
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_pSRV->GetCPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_Image.GetMetadata().format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    md3dDevice->CreateShaderResourceView(m_pTex2D.Get(), &srvDesc, m_pSRV->GetCPUDescriptorHandleForHeapStart());
}



void Init() {
    // 1. Init direct3D
    CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory));
    D3D12CreateDevice(nullptr,             // default adapter
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&md3dDevice));

    md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&mFence));


    // 1-1. Create Command Objects
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));

    md3dDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf()));

    md3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mDirectCmdListAlloc.Get(), // Associated command allocator
        nullptr,                   // Initial PipelineStateObject
        IID_PPV_ARGS(mCommandList.GetAddressOf()));

    // 1-2. Create Swap Chain
    // Release the previous swapchain we will be recreating.

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = mBackBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = SwapChainBufferCount;
    sd.OutputWindow = hWnd;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Note: Swap chain uses queue to perform flush.
    mdxgiFactory->CreateSwapChain(
        mCommandQueue.Get(),
        &sd,
        mSwapChain.GetAddressOf());

    mCurrBackBuffer = 0;

    // 1-3. Create Rtv And Dsv DescriptorHeaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf()));


    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf()));

    // Flush before changing any resources.

    // Resize the swap chain.



    LoadTexture(L"Textures\\WoodCrate01.png");


    // RTV 생성
    mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle;
    rtvHeapHandle.ptr = mRtvHeap->GetCPUDescriptorHandleForHeapStart().ptr;
    for (UINT i = 0; i < SwapChainBufferCount; i++) {
        mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i]));
        md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);

        rtvHeapHandle.ptr += mRtvDescriptorSize;
    }
    // Create the depth/stencil buffer and view.

    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = WIDTH;
    depthStencilDesc.Height = HEIGHT;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

    // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
    // the depth buffer.  Therefore, because we need to create two views to the same resource:
    //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    // we need to create the depth buffer resource with a typeless format.  
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;


    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

    md3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()));


    // Create descriptor to mip level 0 of entire resource using the format of the resource.

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = mDepthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, mDsvHeap->GetCPUDescriptorHandleForHeapStart());


    // Transition the resource from its initial state to be used as a depth buffer.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Start off in a closed state.  This is because the first time we refer 
    // to the command list we will Reset it, and it needs to be closed before
    // calling Reset.
    mCommandList->Close();

    // Execute the resize commands.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until resize is complete.
    FlushCommandQueue();

    // Update the viewport transform to cover the client area.
    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = static_cast<float>(WIDTH);
    mScreenViewport.Height = static_cast<float>(HEIGHT);
    mScreenViewport.MinDepth = 0.0f;
    mScreenViewport.MaxDepth = 1.0f;

    mScissorRect = { 0, 0, WIDTH, HEIGHT };


    // 6. Build Box Geometry

    std::array<Vertex, 4> vertices =
    {
        Vertex({ DirectX::XMFLOAT3(-1.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) , DirectX::XMFLOAT2(0.0f, 0.0f)}),
        Vertex({ DirectX::XMFLOAT3(1.0f, 0.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) , DirectX::XMFLOAT2(0.0f, 1.0f)}),
        Vertex({ DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) , DirectX::XMFLOAT2(1.0f, 1.0f)}),
        Vertex({ DirectX::XMFLOAT3(-1.0f, 0.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) , DirectX::XMFLOAT2(1.0f, 0.0f)})
    };

    std::array<std::uint16_t, 6> indices =
    {
        0, 2, 1,
        0, 3, 2,
    };

    vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);


    md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vbByteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(VertexBufferGPU.GetAddressOf()));

    // 생성된 리소스에 vertices의 정보를 넣어야 한다.
    // Copy the triangle data to the vertex buffer.
    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexReadRange(0, 0); // We do not intend to read from this resource on the CPU.
    VertexBufferGPU->Map(0, &vertexReadRange, &vertexDataBuffer);
    ::memcpy(vertexDataBuffer, &vertices[0], vbByteSize);
    VertexBufferGPU->Unmap(0, nullptr);

    md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(ibByteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(IndexBufferGPU.GetAddressOf()));

    // 생성된 리소스에 indexes의 정보를 넣어야 한다.
    // Copy the triangle data to the index buffer.
    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexReadRange(0, 0); // We do not intend to read from this resource on the CPU.
    IndexBufferGPU->Map(0, &indexReadRange, &indexDataBuffer);
    ::memcpy(indexDataBuffer, &indices[0], ibByteSize);
    IndexBufferGPU->Unmap(0, nullptr);


    // 3. Build Constant Buffers
    md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(ObjectConstants)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mObjectCB));

    md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(MaterialConstants)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mMaterialCB));

    md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(PassConstants)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mPassCB));


    BYTE* ConstantDataBuffer = nullptr;
    CD3DX12_RANGE ConstantReadRange(0, 0); // We do not intend to read from this resource on the CPU.
    mObjectCB->Map(0, nullptr, reinterpret_cast<void**>(&ConstantDataBuffer));

    DirectX::XMFLOAT4X4 tmp
    (0.2f, 0.4f, 0.6f, 0.8f,
        0.2f, 0.4f, 0.6f, 0.8f,
        0.2f, 0.4f, 0.6f, 0.8f,
        0.2f, 0.4f, 0.6f, 0.8f);

    ::memcpy(&ConstantDataBuffer[objCBByteSize], &tmp, sizeof(DirectX::XMFLOAT4X4));
    mObjectCB->Unmap(0, nullptr);
    */

        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 3;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress;
    // Offset to the ith object constant buffer in the buffer.

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;

    D3D12_CPU_DESCRIPTOR_HANDLE mCbvHeapHandle = mCbvHeap->GetCPUDescriptorHandleForHeapStart();
    mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    cbAddress = mObjectCB->GetGPUVirtualAddress();
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = objectCBByteSize;
    md3dDevice->CreateConstantBufferView(
        &cbvDesc,
        mCbvHeapHandle);

    mCbvHeapHandle.ptr += mCbvSrvUavDescriptorSize;

    cbAddress = mMaterialCB->GetGPUVirtualAddress();
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = materialCBByteSize;
    md3dDevice->CreateConstantBufferView(
        &cbvDesc,
        mCbvHeapHandle);

    mCbvHeapHandle.ptr += mCbvSrvUavDescriptorSize;

    cbAddress = mPassCB->GetGPUVirtualAddress();
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = passCBByteSize;
    md3dDevice->CreateConstantBufferView(
        &cbvDesc,
        mCbvHeapHandle);

    // 4. Build Root Signature
    // Shader programs typically require resources as input (constant buffers,
    // textures, samplers).  The root signature defines the resources the shader
    // programs expect.  If we think of the shader programs as a function, and
    // the input resources as function parameters, then the root signature can be
    // thought of as defining the function signature.  

    // Root parameter can be a table, root descriptor or root constants.

    D3D12_ROOT_PARAMETER slotRootParameter[4];

    D3D12_DESCRIPTOR_RANGE descRange[1];

    descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange[0].NumDescriptors = 1;
    descRange[0].BaseShaderRegister = 0;
    descRange[0].RegisterSpace = 0;
    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    slotRootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
    slotRootParameter[0].DescriptorTable.pDescriptorRanges = &descRange[0];

    slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    slotRootParameter[1].Descriptor.ShaderRegister = 0;
    slotRootParameter[1].Descriptor.RegisterSpace = 0;

    slotRootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    slotRootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    slotRootParameter[2].Descriptor.ShaderRegister = 1;
    slotRootParameter[2].Descriptor.RegisterSpace = 0;

    slotRootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    slotRootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    slotRootParameter[3].Descriptor.ShaderRegister = 2;
    slotRootParameter[3].Descriptor.RegisterSpace = 0;


    CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        0, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy


    D3D12_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.NumParameters = 4;
    rootSigDesc.pParameters = slotRootParameter;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &anisotropicClamp;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature));


    // 5. Build Shaders And Input Layout
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr1 = D3DCompileFromFile(L"Shaders\\Default.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VS", "vs_5_0", compileFlags, 0, &mvsByteCode, &errors);
    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    HRESULT hr2 = D3DCompileFromFile(L"Shaders\\Default.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PS", "ps_5_0", compileFlags, 0, &mpsByteCode, &errors);
    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,   DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };


    // 7. Build PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO));

    //===============================

    // Execute the initialization commands.
    mCommandList->Close();
    ID3D12CommandList* cmdsLists_[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists_), cmdsLists_);

    // Wait until initialization is complete.
    FlushCommandQueue();

}


void Update() {
    float x = mRadius * sinf(mPhi) * sinf(mTheta);
    float z = mRadius * sinf(mPhi) * cosf(mTheta);
    float y = mRadius * cosf(mPhi);

    DirectX::XMVECTOR position = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR org = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1.0, 0, 0.0);

    PassConstants mPassConstants;

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(position, org, up);
    DirectX::XMStoreFloat4x4(&mPassConstants.gView, DirectX::XMMatrixTranspose(view));

    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, (float)WIDTH / (float)HEIGHT, 0.1f, 50);
    DirectX::XMStoreFloat4x4(&mPassConstants.gViewProj, DirectX::XMMatrixTranspose(proj));

    mPassConstants.gLights[0].Strength = { 0.5f, 0.5f, 0.5f };
    mPassConstants.gLights[0].FalloffStart = 1.0f;
    mPassConstants.gLights[0].Direction = { 0.0f, -1.0f, 0.0f };
    mPassConstants.gLights[0].FalloffEnd = 10.0f;
    mPassConstants.gLights[0].Position = { 0.0f, 0.0f, 0.0f };
    mPassConstants.gLights[0].SpotPower = 64.0f;

    mPassConstants.AmbientLight = { 0.2f, 0.2f, 0.2f, 1.0f };

    mPassConstants.EyePosW = { x,y,z };

    BYTE* mappingDataBuffer = nullptr;

    D3D12_RANGE dataReadRange;
    dataReadRange.Begin = 0;
    dataReadRange.End = 0;

    static int i = 0;
    i++;
    if (i == 3) {
        int x;
        x = 5;
    }

    ObjectConstants mObjectConstants;
    DirectX::XMMATRIX boxWorld = DirectX::XMMatrixIdentity();
    DirectX::XMStoreFloat4x4(&mObjectConstants.gWorld, DirectX::XMMatrixTranspose(boxWorld));

    mObjectCB->Map(0, &dataReadRange, reinterpret_cast<void**>(&mappingDataBuffer));
    ::memcpy(mappingDataBuffer, &mObjectConstants, sizeof(ObjectConstants));
    mObjectCB->Unmap(0, nullptr);

    MaterialConstants mMaterialConstants;

    mMaterialCB->Map(0, &dataReadRange, reinterpret_cast<void**>(&mappingDataBuffer));
    ::memcpy(mappingDataBuffer, &mMaterialConstants, sizeof(MaterialConstants));
    mMaterialCB->Unmap(0, nullptr);

    mPassCB->Map(0, &dataReadRange, reinterpret_cast<void**>(&mappingDataBuffer));
    ::memcpy(mappingDataBuffer, &mPassConstants, sizeof(PassConstants));
    mPassCB->Unmap(0, nullptr);



    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    mDirectCmdListAlloc->Reset();

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get());


    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());


    ID3D12DescriptorHeap* descriptorHeaps[] = { m_pSRV.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootDescriptorTable(0, m_pSRV->GetGPUDescriptorHandleForHeapStart());
    mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(2, mMaterialCB->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(3, mPassCB->GetGPUVirtualAddress());

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    CD3DX12_CPU_DESCRIPTOR_HANDLE cBackBufferViewHandle(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize);
    mCommandList->ClearRenderTargetView(cBackBufferViewHandle, DirectX::Colors::Black, 0, nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &cBackBufferViewHandle, true, &dsvHandle);


    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
    vbv.StrideInBytes = sizeof(Vertex);
    vbv.SizeInBytes = vbByteSize;
    mCommandList->IASetVertexBuffers(0, 1, &vbv);
    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
    ibv.Format = DXGI_FORMAT_R16_UINT;
    ibv.SizeInBytes = ibByteSize;
    mCommandList->IASetIndexBuffer(&ibv);
    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->DrawIndexedInstanced(ibByteSize / sizeof(std::uint16_t), 1, 0, 0, 0);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    mCommandList->Close();

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    mSwapChain->Present(0, 0);
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}




int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance
    , LPSTR lpszCmdParam, int nCmdShow)
{
    g_hInst = hInstance;

    WndClass.cbClsExtra = 0;
    WndClass.cbWndExtra = 0;
    WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    WndClass.hInstance = hInstance;
    WndClass.lpfnWndProc = (WNDPROC)WndProc;
    WndClass.lpszClassName = lpszClass;
    WndClass.lpszMenuName = NULL;
    WndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    RegisterClass(&WndClass);

    RECT rt = { 0,0,WIDTH,HEIGHT };
    AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, FALSE);


    width = rt.right - rt.left;
    height = rt.bottom - rt.top;

    hWnd = CreateWindow(lpszClass, lpszClass, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, (HMENU)NULL, hInstance, NULL);
    ShowWindow(hWnd, nCmdShow);

    Init();

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIENT));

    while (true) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;

            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
            Update();
    }
    return msg.wParam;
}

int lastMousePosX, lastMousePosY;
bool isLButtonDown, isRButtonDown;
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage) {
    case WM_CREATE:
        isLButtonDown = false;
        isRButtonDown = false;
        return 0;

    case WM_LBUTTONDOWN:
        isLButtonDown = true;
        lastMousePosX = LOWORD(lParam);
        lastMousePosY = HIWORD(lParam);
        return 0;


    case WM_RBUTTONDOWN:
        isRButtonDown = true;
        lastMousePosY = HIWORD(lParam);
        return 0;


    case WM_LBUTTONUP:
        isLButtonDown = false;
        return 0;


    case WM_RBUTTONUP:
        isRButtonDown = false;
        return 0;

    case WM_MOUSEMOVE: {
        int y = HIWORD(lParam);
        float dy = -DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePosY));

        if (isLButtonDown == true) {
            int x = LOWORD(lParam);
            float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePosX));

            mTheta += dx;
            lastMousePosX = x;

            mPhi += dy;

            if (mPhi >= DirectX::XM_PI) mPhi = DirectX::XM_PI - 0.001f;
            if (mPhi <= 0)              mPhi = 0.001f;

            lastMousePosY = y;
        }

        if (isRButtonDown == true) {
            mRadius += -2 * dy;

            if (mRadius <= 3.f) mRadius = 3.f;
            if (mRadius >= 10.0f) mRadius = 10.0f;

            lastMousePosY = y;
        }
        return 0;
    }


    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return(DefWindowProc(hWnd, iMessage, wParam, lParam));
}
