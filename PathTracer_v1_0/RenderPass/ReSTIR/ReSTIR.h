#pragma once
#include "imgui.h"
#include "Framework.h"
#include "d3d12shader.h"

enum ReSTIR_MODE : int
{
	ReSTIR_MODE_NO_REUSE = 0,
	ReSTIR_MODE_TEMPORAL_REUSE = 1,
	ReSTIR_MODE_SPATIAL_REUSE = 2,
	ReSTIR_MODE_SPATIOTEMPORAL_REUSE = 3
};

struct ReSTIRParameters
{
	int enabled;
	int resamplingMode;

	int lightCandidateCount;
	int maxHistoryLength;

	float normalThreshold;
	float depthThreshold;
};


class ReSTIR
{
public:
	ReSTIR(ID3D12Device5Ptr mpDevice);
	void processGUI();
	void uploadParams();

	bool mEnabled = 1;

	ID3D12Device5Ptr mpDevice;
	ReSTIRParameters param;
	ReSTIRParameters defaultParam;
};