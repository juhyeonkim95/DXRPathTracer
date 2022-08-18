#pragma once
#include "tiny_obj_loader.h"
#include "Framework.h"
#include <vector>
using namespace std;

class Mesh
{
public:
	Mesh();
	Mesh(mat4 transform, const std::string& meshType);
	Mesh(const char* filepath);
	Mesh(tinyobj::ObjReader* reader);
	// Accessors
	const int getVerticesNumber() const { return verticesNumber; }
	const int getIndicesNumber() const { return indicesNumber; }

	const ID3D12ResourcePtr getVerticesBuffer() const { return pBuffer; }
	const ID3D12ResourcePtr getIndicesBuffer() const { return iBuffer; }
	void createMeshBuffer(ID3D12Device5Ptr pDevice);

	const vec3* vertices = nullptr;
	const vec3* normals = nullptr;
	const vec2* texcoords = nullptr;
	const uint32* indices = nullptr;
	uint32 verticesNumber;
	uint32 indicesNumber;
	uint32 indicesOffset;
	uint32 verticesOffset;
	ID3D12ResourcePtr pBuffer;
	ID3D12ResourcePtr iBuffer;
	mat4 transform;

	int lightIndex = -1;
};