#include "Mesh.h"
#include "DX12BufferUtils.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

Mesh::Mesh()
{
    this->vertices = new vec3[4]
    {
        vec3(0.5f,0.5f,0),
        vec3(0.5f,-0.5f,0),
        vec3(-0.5f,-0.5f,0),
        vec3(-0.5f,0.5f,0)
    };
    this->indices = new uint32[6]
    {
        0, 1, 2,
        0, 2, 3
    };
    this->verticesNumber = 4;
    this->indicesNumber = 6;
}

Mesh::Mesh(mat4 transform, const std::string &meshType)
{
    if (strcmp(meshType.c_str(), "rectangle") == 0) {
        vec3* verticesOriginal = new vec3[4]
        {
            vec3(1.0f,1.0f,0),
            vec3(1.0f,-1.0f,0),
            vec3(-1.0f,-1.0f,0),
            vec3(-1.0f,1.0f,0)
        };

        this->texcoords = new vec2[4]
        {
            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f)
        };

        vec3* normalsOriginal = new vec3[4]
        {
            vec3(0, 0, 1.0f),
            vec3(0, 0, 1.0f),
            vec3(0, 0, 1.0f),
            vec3(0, 0, 1.0f),
        };

        for (int i = 0; i < 4; i++) {
            vec3 transformedVector = vec3(transform * vec4(verticesOriginal[i], 1.0f));
            verticesOriginal[i] = transformedVector;
        }

        for (int i = 0; i < 4; i++) {
            vec3 transformedVector = vec3(transform * vec4(normalsOriginal[i], 0.0f));
            normalsOriginal[i] = transformedVector;
        }

        this->vertices = verticesOriginal;
        this->normals = normalsOriginal;

        this->indices = new uint32[6]
        {
            0, 1, 2,
            0, 2, 3
        };

        this->verticesNumber = 4;
        this->indicesNumber = 6;
    }
    else if (strcmp(meshType.c_str(), "cube") == 0)
    {
        vec3* verticesOriginal = new vec3[24]
        {
            vec3(-1.0f, 1.0f, -1.0f), 
            vec3(1.0f, 1.0f, -1.0f), 
            vec3(1.0f, 1.0f, 1.0f), 
            vec3(-1.0f, 1.0f, 1.0f), 

            vec3(-1.0f, -1.0f, -1.0f), 
            vec3(1.0f, -1.0f, -1.0f), 
            vec3(1.0f, -1.0f, 1.0f), 
            vec3(-1.0f, -1.0f, 1.0f), 

            vec3(-1.0f, -1.0f, 1.0f), 
            vec3(-1.0f, -1.0f, -1.0f), 
            vec3(-1.0f, 1.0f, -1.0f), 
            vec3(-1.0f, 1.0f, 1.0f), 

            vec3(1.0f, -1.0f, 1.0f), 
            vec3(1.0f, -1.0f, -1.0f), 
            vec3(1.0f, 1.0f, -1.0f), 
            vec3(1.0f, 1.0f, 1.0f),

            vec3(-1.0f, -1.0f, -1.0f), 
            vec3(1.0f, -1.0f, -1.0f), 
            vec3(1.0f, 1.0f, -1.0f), 
            vec3(-1.0f, 1.0f, -1.0f), 

            vec3(-1.0f, -1.0f, 1.0f), 
            vec3(1.0f, -1.0f, 1.0f), 
            vec3(1.0f, 1.0f, 1.0f), 
            vec3(-1.0f, 1.0f, 1.0f)
        };

        vec3* normalsOriginal = new vec3[24]
        {
            vec3(0.0f, 1.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f),

            vec3(0.0f, -1.0f, 0.0f),
            vec3(0.0f, -1.0f, 0.0f),
            vec3(0.0f, -1.0f, 0.0f),
            vec3(0.0f, -1.0f, 0.0f),

            vec3(-1.0f, 0.0f, 0.0f),
            vec3(-1.0f, 0.0f, 0.0f),
            vec3(-1.0f, 0.0f, 0.0f),
            vec3(-1.0f, 0.0f, 0.0f),

            vec3(1.0f, 0.0f, 0.0f),
            vec3(1.0f, 0.0f, 0.0f),
            vec3(1.0f, 0.0f, 0.0f),
            vec3(1.0f, 0.0f, 0.0f),

            vec3(0.0f, 0.0f, -1.0f),
            vec3(0.0f, 0.0f, -1.0f),
            vec3(0.0f, 0.0f, -1.0f),
            vec3(0.0f, 0.0f, -1.0f),

            vec3(0.0f, 0.0f, 1.0f),
            vec3(0.0f, 0.0f, 1.0f),
            vec3(0.0f, 0.0f, 1.0f),
            vec3(0.0f, 0.0f, 1.0f),
        };

        this->texcoords = new vec2[24]
        {
            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f),

            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f),

            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f),

            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f),

            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f),

            vec2(1.0f, 1.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 0.0f),
            vec2(0.0f, 1.0f)
        };

        for (int i = 0; i < 24; i++) {
            vec3 transformedVector = vec3(transform * vec4(verticesOriginal[i], 1.0f));
            verticesOriginal[i] = transformedVector;
        }

        for (int i = 0; i < 24; i++) {
            vec3 transformedVector = vec3(transform * vec4(normalsOriginal[i], 0.0f));
            normalsOriginal[i] = transformedVector;
        }

        this->vertices = verticesOriginal;
        this->normals = normalsOriginal;

        this->indices = new uint32[48]
        {
            3,1,0,
            2,1,3,

            6,4,5,
            7,4,6,

            11,9,8,
            10,9,11,

            14,12,13,
            15,12,14,

            19,17,16,
            18,17,19,

            22,20,21,
            23,20,22
        };

        this->verticesNumber = 24;
        this->indicesNumber = 48;
    }
    
}

Mesh::Mesh(const char* filename)
{
    std::vector<vec3> *vertices = new vector<vec3>();
    std::vector<uint32> *indices = new vector<uint32>();
    std::vector<vec3> normals;
    std::vector<vec2> texcoords;

    string line;
    ifstream myfile(filename);

    if (myfile.is_open())
    {
        while (getline(myfile, line))
        {
            cout << line << '\n';
            vector<string> out;
            //tokenize(line, ' ', out);
            string lineInfoType = out.at(0);

            if (lineInfoType.compare("v") == 0) {
                float x = atof(out.at(1).c_str());
                float y = atof(out.at(2).c_str());
                float z = atof(out.at(3).c_str());
                vertices->push_back(vec3(x, y, z));
            }
            else if (lineInfoType.compare("vn") == 0) {
                float x = atof(out.at(1).c_str());
                float y = atof(out.at(2).c_str());
                float z = atof(out.at(3).c_str());
                normals.push_back(vec3(x, y, z));
            }
            else if (lineInfoType.compare("vt") == 0) {
                float x = atof(out.at(1).c_str());
                float y = atof(out.at(2).c_str());
                texcoords.push_back(vec2(x, y));
            }
            else if (lineInfoType.compare("f") == 0) {
                vector<string> vertexInfo;
                //tokenize(out.at(1), '/', vertexInfo);
                uint32 v1 = atoi(vertexInfo.at(0).c_str());
                //tokenize(out.at(2), '/', vertexInfo);
                uint32 v2 = atoi(vertexInfo.at(0).c_str());
                //tokenize(out.at(3), '/', vertexInfo);
                uint32 v3 = atoi(vertexInfo.at(0).c_str());
                indices->push_back(v1 - 1);
                indices->push_back(v2 - 1);
                indices->push_back(v3 - 1);
            }
        }
        myfile.close();
    }
    else cout << "Unable to open file";
    this->vertices = vertices->data();
    this->indices = indices->data();
    this->verticesNumber = vertices->size();
    this->indicesNumber = indices->size();
}

Mesh::Mesh(tinyobj::ObjReader* reader)
{
    auto& shapes = reader->GetShapes();
    auto& attrib = reader->GetAttrib();
    
    tinyobj::shape_t shape = shapes[0];
    std::vector<vec3>* vertices = new vector<vec3>(attrib.vertices.size() / 3);
    std::vector<vec3>* normals = new vector<vec3>(attrib.normals.size() / 3);
    std::vector<vec2>* texcoords = new vector<vec2>(attrib.texcoords.size() / 2);

    std::vector<uint32>* indices = new vector<uint32>(shape.mesh.indices.size());
    memcpy(vertices->data(), attrib.vertices.data(), attrib.vertices.size() * sizeof(float));
    memcpy(normals->data(), attrib.normals.data(), attrib.normals.size() * sizeof(float));
    memcpy(texcoords->data(), attrib.texcoords.data(), attrib.texcoords.size() * sizeof(float));

    for (int i = 0; i < shape.mesh.indices.size(); i++) {
        int vertex_index = shape.mesh.indices[i].vertex_index;
        int normal_index = shape.mesh.indices[i].normal_index;
        int texcoord_index = shape.mesh.indices[i].texcoord_index;
        assert(vertex_index == normal_index);
        assert(vertex_index == texcoord_index);

        indices->at(i) = shape.mesh.indices[i].vertex_index;

    }
    this->vertices = vertices->data();
    this->normals = normals->data();
    this->texcoords = texcoords->data();
    this->indices = indices->data();
    this->verticesNumber = attrib.vertices.size() / 3;
    this->indicesNumber = indices->size();
}


void Mesh::createMeshBuffer(ID3D12Device5Ptr pDevice)
{
    // Create vertices buffer
    pBuffer = createBuffer(pDevice, verticesNumber * sizeof(vec3), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    pBuffer->Map(0, nullptr, (void**)&pData);
    memcpy(pData, vertices, verticesNumber * sizeof(vec3));
    pBuffer->Unmap(0, nullptr);

    // Create indices buffer
    iBuffer = createBuffer(pDevice, indicesNumber * sizeof(uint32), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* iData;
    iBuffer->Map(0, nullptr, (void**)&iData);
    memcpy(iData, indices, indicesNumber * sizeof(uint32));
    iBuffer->Unmap(0, nullptr);
}
