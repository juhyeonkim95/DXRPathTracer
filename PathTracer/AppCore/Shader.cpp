#include "Shader.h"

Shader::Shader(const wchar_t* vertexShaderPath, const wchar_t* pixelShaderPath, ID3D12Device5Ptr pDevice, int inputTextureNumber, std::vector<DXGI_FORMAT> &rtvFormats, int inputCBVNumber)
{
    this->mpDevice = pDevice;
    this->createRootSignature(inputTextureNumber, inputCBVNumber);
    this->compileShaderFile(vertexShaderPath, pixelShaderPath, rtvFormats);
}

void Shader::compileShaderFile(const wchar_t* vertexShaderPath, const wchar_t* pixelShaderPath, std::vector<DXGI_FORMAT>& rtvFormats)
{
    HRESULT hr;

    // compile vertex shader
    ID3DBlob* vertexShader; // d3d blob for holding vertex shader bytecode
    ID3DBlob* errorBuff; // a buffer holding the error data if any
    hr = D3DCompileFromFile(vertexShaderPath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vertexShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
    }

    // fill out a shader bytecode structure, which is basically just a pointer
    // to the shader bytecode and the size of the shader bytecode
    D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
    vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
    vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

    // compile pixel shader
    ID3DBlob* pixelShader;
    hr = D3DCompileFromFile(pixelShaderPath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &pixelShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
    }

    // fill out shader bytecode structure for pixel shader
    D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
    pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
    pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // fill out an input layout description structure
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    // we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
    inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
    inputLayoutDesc.pInputElementDescs = inputLayout;


    // create a pipeline state object (PSO)

    // In a real application, you will have many pso's. for each different shader
    // or different combinations of shaders, different blend states or different rasterizer states,
    // different topology types (point, line, triangle, patch), or a different number
    // of render targets you will need a pso

    // VS is the only required shader for a pso. You might be wondering when a case would be where
    // you only set the VS. It's possible that you have a pso that only outputs data with the stream
    // output, and not on a render target, which means you would not need anything after the stream
    // output.

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
    psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
    psoDesc.pRootSignature = mpRootSignature; // the root signature that describes the input data this pso needs
    psoDesc.VS = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is
    psoDesc.PS = pixelShaderBytecode; // same as VS but for pixel shader
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing

    for (int i = 0; i < rtvFormats.size(); i++) 
    {
        psoDesc.RTVFormats[i] = rtvFormats[i];
    }
    // psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the render target
    psoDesc.NumRenderTargets = rtvFormats.size(); // we are only binding one render target

    psoDesc.SampleDesc.Count = 1; // must be the same sample description as the swapchain and depth/stencil buffer
    psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blend state.
    
    // create the pso
    hr = mpDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpPipelineStateObject));
    if (FAILED(hr)) {
        throw std::invalid_argument("Shader Compile Failed!");
    }
}

void Shader::createRootSignature(int inputTextureNumber, int inputCBVNumber)
{
    // create root signature

    // create a root descriptor, which explains where to find the data for this root parameter
    std::vector<D3D12_ROOT_DESCRIPTOR> rootCBVDescriptors;
    for (int i = 0; i < inputCBVNumber; i++)
    {
        D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
        rootCBVDescriptor.RegisterSpace = 0;
        rootCBVDescriptor.ShaderRegister = i;
        rootCBVDescriptors.push_back(rootCBVDescriptor);
    }

    // create a descriptor range (descriptor table) and fill it out
    // this is a range of descriptors inside a descriptor heap
    std::vector<D3D12_DESCRIPTOR_RANGE> descriptorTableRanges;
    descriptorTableRanges.resize(inputTextureNumber);

    for (int i = 0; i < inputTextureNumber; i++) {
        descriptorTableRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // this is a range of shader resource views (descriptors)
        descriptorTableRanges[i].NumDescriptors = 1; // we only have one texture right now, so the range is only 1
        descriptorTableRanges[i].BaseShaderRegister = i; // start index of the shader registers in the range
        descriptorTableRanges[i].RegisterSpace = 0; // space 0. can usually be zero
        descriptorTableRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables
    }
    std::vector< D3D12_ROOT_DESCRIPTOR_TABLE> descriptorTables;
    descriptorTables.resize(inputTextureNumber);
    for (int i = 0; i < inputTextureNumber; i++) {
        // create a descriptor table
        D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
        descriptorTables[i].NumDescriptorRanges = 1; //_countof(descriptorTableRanges); // we only have one range
        descriptorTables[i].pDescriptorRanges = &descriptorTableRanges[i]; // the pointer to the beginning of our ranges array
    }

    std::vector< D3D12_ROOT_PARAMETER> rootParameters;
    rootParameters.resize(inputTextureNumber + inputCBVNumber);

    // create a root parameter for the root descriptor and fill it out
    for (int i = 0; i < inputCBVNumber; i++)
    {
        rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // this is a constant buffer view root descriptor
        rootParameters[i].Descriptor = rootCBVDescriptors.at(i); // this is the root descriptor for this root parameter
        rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now
    }
    
    for (int i = 0; i < inputTextureNumber; i++) {
        // fill out the parameter for our descriptor table. Remember it's a good idea to sort parameters by frequency of change. Our constant
        // buffer will be changed multiple times per frame, while our descriptor table will not be changed at all (in this tutorial)
        rootParameters[i + inputCBVNumber].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
        rootParameters[i + inputCBVNumber].DescriptorTable = descriptorTables[i]; // this is our descriptor table for this root parameter
        rootParameters[i + inputCBVNumber].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now
    }

    // create a static sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    // sampler.Filter = D3D12_FILTER_ANISOTROPIC;
    // sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_STATIC_SAMPLER_DESC bilinearClamp(
        0,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,
        4
    );

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(rootParameters.size(), // we have 2 root parameters
        rootParameters.data(), // a pointer to the beginning of our root parameters array
        1, // we have one static sampler
        &sampler, // a pointer to our static sampler (array)
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // we can deny shader stages here for better performance
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    ID3DBlobPtr signature;
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    mpDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mpRootSignature));
}