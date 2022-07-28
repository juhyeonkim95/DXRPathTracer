#pragma once
#include "Framework.h"

class HeapData
{
public:
	ID3D12DescriptorHeapPtr pHeap;
	uint32_t usedEntries = 0;
};