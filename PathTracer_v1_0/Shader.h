#include <string>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include "d3dx12.h"
#include "Framework.h"

class Shader
{
public:
	Shader() {};
	Shader(const wchar_t* vertexShaderPath, const wchar_t* pixelShaderPath, ID3D12Device5Ptr mpDevice, int inputTextureNumber);
	ID3D12PipelineState* getPipelineStateObject() { return pipelineStateObject; };
	ID3D12RootSignature* getRootSignature() { return rootSignature; };
private:
	void createRootSignature(int inputTextureNumber);
	void compileShaderFile(const wchar_t* vertexShaderPath, const wchar_t* pixelShaderPath);
	
	ID3D12PipelineState* pipelineStateObject;
	ID3D12RootSignature* rootSignature;
	ID3D12Device5Ptr mpDevice;
};