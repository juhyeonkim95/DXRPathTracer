#pragma once
#include "imgui.h"
#include "Framework.h"
#include "d3d12shader.h"

struct PathTracerParameters
{
	int maxDepth;
	int maxDepthDiffuse;
	int maxDepthSpecular;
	int maxDepthTransmittance;

	bool accumulate;
	int unused1;
	int unused2;
	int unused3;
};


class PathTracer
{
public:
	PathTracer(ID3D12Device5Ptr mpDevice);
	void processGUI();

	ID3D12Device5Ptr mpDevice;
	PathTracerParameters param;
	PathTracerParameters defaultParam;
	bool mDirty = false;
};