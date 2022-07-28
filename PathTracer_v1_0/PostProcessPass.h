#pragma once
#include "Framework.h"
#include "Shader.h"
#include "RenderTexture.h"
#include <map>
using namespace std;

class PostProcessPass {
public:
	PostProcessPass(ID3D12Device5Ptr mpDevice, uvec2 size) 
	{ 
		this->mpDevice = mpDevice; 
		this->size = size; 
	};

	ID3D12Device5Ptr mpDevice;
	uvec2 size;
};