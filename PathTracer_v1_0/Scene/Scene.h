#pragma once
#include "Framework.h"
#include <vector>
#include "Shape/Mesh.h"
#include "BSDF/BSDF.h"
#include <map>
#include "tinyxml2.h"
#include <filesystem>
#include "Sensor/Sensor.h"
#include "Texture/Texture.h"
#include "CommonStruct.h"
namespace fs = std::filesystem;

using namespace std;
using namespace tinyxml2;



class Scene 
{
public:
	Scene();
	Scene(const char* sceneName);
	// Accessors
	vector<Mesh>& getMeshes() { return meshes;  }
	Sensor* getSensor() { return this->sensor; }
	//map<string, BSDF*> getMaterialDictionary() { return this->materialDictionary; }

	fs::path sceneDirectory;
	vector<Mesh> meshes;
	vector<string> meshRefID;
	vector<BSDF*> bsdfs;
	vector<Texture*> textures;
	vector<LightParameter> lights;
	map<string, int> materialIDDictionary;
	map<string, int> textureIDDictionary;

	const char* sceneName;
	XMLElement* root;
	Sensor* sensor;
	void loadShape();
	void loadShapeNew();
	void loadTexture();

	void loadSensor();
	void loadBSDFs();
	void loadTextures();
	void loadEmitters();
	
	Texture* envMapTexture;
	mat4 envMapTransform;

	uint32 verticesNumber=0;
	uint32 indicesNumber=0;
};