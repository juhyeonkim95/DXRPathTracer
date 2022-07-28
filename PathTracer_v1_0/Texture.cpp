#pragma warning(disable:4996)
#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include "stb_image.h"
#include <iostream>

const wchar_t* GetErrorDesc(HRESULT hr)
{
    static wchar_t desc[1024] = {};

    LPWSTR errorText = nullptr;

    const DWORD result = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        nullptr, static_cast<DWORD>(hr),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&errorText), 0, nullptr);

    *desc = 0;

    if (result > 0 && errorText)
    {
        swprintf_s(desc, L": %ls", errorText);

        size_t len = wcslen(desc);
        if (len >= 1)
        {
            desc[len - 1] = 0;
        }

        if (errorText)
            LocalFree(errorText);
    }

    return desc;
}

std::string getFileExt(const std::string& s) {

    size_t i = s.rfind('.', s.length());
    if (i != std::string::npos) {
        return(s.substr(i + 1, s.length() - i));
    }

    return("");
}

Texture::Texture(const char* filePath)
{
	//this->data = stbi_load(filePath, &width, &height, &nrChannels, 0);
	//std::cout << "Width: " << width << std::endl;
	//std::cout << "Height: " << height << std::endl;
	//std::cout << "Channel: " << nrChannels << std::endl;
    this->name = filePath;
    std::string ext = getFileExt(std::string(filePath));
	TEX_FILTER_FLAGS dwFilter = TEX_FILTER_DEFAULT;
	WIC_FLAGS wicFlags = WIC_FLAGS_NONE | dwFilter;
	TexMetadata info;
	std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);
	const size_t cSize = strlen(filePath) + 1;
	std::wstring wc(cSize, L'#');
	mbstowcs(&wc[0], filePath, cSize);
    std::cout << "[Texture loading] " << filePath << std::endl;

    if (strcmp(ext.c_str(), "hdr") == 0) {
        HRESULT hr = LoadFromHDRFile(&wc[0], &info, *image);
        this->textureImage = std::move(image);
    }
    else {
        HRESULT hr = LoadFromWICFile(&wc[0], wicFlags, &info, *image);
        this->textureImage = std::move(image);
    }

    //std::cout << "Width: " << this->textureImage->GetImage(0,0,0)->width << std::endl;
    //std::cout << "Height: " << this->textureImage->GetImage(0, 0, 0)->height << std::endl;

}