#pragma once
#include "DirectXTex.h"
using namespace DirectX;

class Texture {
public:
	Texture() {};
	Texture(const char* filePath);
	int width, height, nrChannels;
	unsigned char* data;
	std::unique_ptr<ScratchImage> textureImage;
	// uint32_t descriptorHandleOffset;
	std::string name;
};