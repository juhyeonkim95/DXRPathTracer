#pragma once
#include "PostProcessPass.h"

enum class ToneMapperOperator : int
{
	Linear,             ///< Linear mapping
	Reinhard,           ///< Reinhard operator
	ReinhardModified,   ///< Reinhard operator with maximum white intensity
	HejiHableAlu,       ///< John Hable's ALU approximation of Jim Heji's filmic operator
	HableUc2,           ///< John Hable's filmic tone-mapping used in Uncharted 2
	Aces,               ///< Aces Filmic Tone-Mapping
};

struct TonemapParameters
{
	int mode;
	int srgbConversion;
	float whiteScale;
	float whiteMaxLuminance;
};

class ToneMapper : public PostProcessPass
{
public:
	ToneMapper(ID3D12Device5Ptr mpDevice, uvec2 size);
	void createRenderTextures(
		ID3D12DescriptorHeapPtr pRTVHeap,
		uint32_t& usedRTVHeapEntries,
		ID3D12DescriptorHeapPtr pSRVHeap,
		uint32_t& usedSRVHeapEntries);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

private:
	Shader* mpShader;
	TonemapParameters mParam;
	TonemapParameters mDefaultParam;
	ID3D12ResourcePtr mpParameterBuffer = nullptr;

	void uploadParams();
};