#include "ComputeShader.h"

ComputeShader::ComputeShader(const wchar_t* computeShaderPath, ID3D12Device5Ptr mpDevice, int inputTextureNumber)
{
    this->mpDevice = mpDevice;
    this->createRootSignature(inputTextureNumber);
    this->compileShaderFile(computeShaderPath);
}

void ComputeShader::compileShaderFile(const wchar_t* computeShaderPath)
{
    HRESULT hr;

    // compile vertex shader
    ID3DBlob* computeShader; // d3d blob for holding vertex shader bytecode
    ID3DBlob* errorBuff; // a buffer holding the error data if any
    hr = D3DCompileFromFile(computeShaderPath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "cs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &computeShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
    }

}

void ComputeShader::createRootSignature(int inputTextureNumber)
{
    
}