#include <string>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include "d3dx12.h"
#include "Framework.h"
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

class Shader
{
public:
	Shader() {};
	Shader(const wchar_t* vertexShaderPath, const wchar_t* pixelShaderPath, ID3D12Device5Ptr mpDevice, int inputTextureNumber, std::vector<DXGI_FORMAT> &rtvFormats, int inputCBVNumber = 1);
	ID3D12PipelineState* getPipelineStateObject() { return pipelineStateObject; };
	ID3D12RootSignature* getRootSignature() { return rootSignature; };
private:
	void createRootSignature(int inputTextureNumber, int inputCBVNumber);
	void compileShaderFile(const wchar_t* vertexShaderPath, const wchar_t* pixelShaderPath, std::vector<DXGI_FORMAT>& rtvFormats);
	
	ID3D12PipelineState* pipelineStateObject;
	ID3D12RootSignature* rootSignature;
	ID3D12Device5Ptr mpDevice;
};