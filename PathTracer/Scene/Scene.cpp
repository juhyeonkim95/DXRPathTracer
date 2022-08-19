//#define TINYOBJLOADER_IMPLEMENTATION
#include "Scene.h"
#include "loader_utils.h"
#include "loader_bsdf.h"
#include <iostream>
#include "Sensor/Perspective.h"
#include "light_utils.h"

Scene::Scene()
{
	this->meshes.push_back(Mesh());
	this->sceneName = "default";
}

Scene::Scene(const char* sceneName)
{
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	this->sceneName = sceneName;
	fs::path totalDirectory = "../../Scenes";
	sceneDirectory = totalDirectory / sceneName;

	tinyxml2::XMLDocument doc;
	fs::path xmlFilePath = sceneDirectory / "scene.xml";
	doc.LoadFile(xmlFilePath.string().c_str());
	this->root = doc.FirstChildElement("scene");
	
	this->mSceneTotalLoadTimer = new Timer("Scene Load");
	this->loadShape();
	this->mSceneTotalLoadTimer->addRecord("Load Shape");
	this->loadSensor();
	this->mSceneTotalLoadTimer->addRecord("Load Sensor");
	this->loadBSDFs();
	this->mSceneTotalLoadTimer->addRecord("Load BSDF");
	this->loadEmitters();
	this->mSceneTotalLoadTimer->addRecord("Load Emitters");
	this->loadTextures();
	this->mSceneTotalLoadTimer->addRecord("Load Textures");
	
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	mScneLoadElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

void Scene::loadShapeNew() 
{
	for (XMLElement* e = root->FirstChildElement("shape"); e != NULL; e = e->NextSiblingElement("shape")) {
		const char* shapeType;
		e->QueryStringAttribute("type", &shapeType);
		
	}
}

void Scene::loadShape()
{
	mSceneMeshLoadTimer = new Timer("Scene Mesh Load");
	mSceneTotalLoadTimer->addChildTimer(mSceneMeshLoadTimer);

	for (XMLElement* e = root->FirstChildElement("shape"); e != NULL; e = e->NextSiblingElement("shape")) {
		const char* shapeType;
		e->QueryStringAttribute("type", &shapeType);
		if (strcmp(shapeType, "rectangle") == 0) {
			mat4 transform = loadMatrix4(e->FirstChildElement("transform")->FirstChildElement("matrix"));
			Mesh mesh(transform, "rectangle");
			mesh.transform = mat4();
			this->meshes.push_back(mesh);

			XMLElement* emitter = e->FirstChildElement("emitter");
			if (emitter) {
				this->meshes.at(this->meshes.size()-1).lightIndex = this->lights.size();
				LightParameter param = getLightParameter(transform);
				param.emission = vec4(getVec3ByName(emitter, "radiance", vec3(1,1,1)), 1.0f);
				std::cout << "RADIANCE: " << param.emission.x << std::endl;

				this->lights.push_back(param);
			}

			mSceneMeshLoadTimer->addRecord("rectangle");
		}
		else if (strcmp(shapeType, "cube") == 0) {
			mat4 transform = loadMatrix4(e->FirstChildElement("transform")->FirstChildElement("matrix"));
			Mesh mesh(transform, "cube");
			this->meshes.push_back(mesh);

			mSceneMeshLoadTimer->addRecord("cube");
		}
		else if (strcmp(shapeType, "obj") == 0) {
			const char* objFilePath;
			e->FirstChildElement("string")->QueryStringAttribute("value", &objFilePath);
			fs::path fullPath = sceneDirectory / objFilePath;
			tinyobj::ObjReader reader;
			tinyobj::ObjReaderConfig readerConfig;
			reader.ParseFromFile(fullPath.string(), readerConfig);
			Mesh mesh(&reader);
			mat4 transform = loadMatrix4(e->FirstChildElement("transform")->FirstChildElement("matrix"));
			mesh.transform = transform;
			this->meshes.push_back(mesh);

			mSceneMeshLoadTimer->addRecord(objFilePath);
		}
		if (e->FirstChildElement("ref")) {
			const char* refID;
			e->FirstChildElement("ref")->QueryStringAttribute("id", &refID);
			this->meshRefID.push_back(refID);
		}
		else {
			this->meshRefID.push_back("empty");
		}

	}
}

void Scene::loadSensor()
{
	XMLElement* e = root->FirstChildElement("sensor");
	this->sensor = new Perspective(e);
}

void Scene::loadBSDFs()
{
	for (XMLElement* e = root->FirstChildElement("bsdf"); e != NULL; e = e->NextSiblingElement("bsdf")) {
		XMLElement* target_e = e;
		const char* bsdfType;
		e->QueryStringAttribute("type", &bsdfType);

		if (strcmp(bsdfType, "bumpmap") == 0) {
			target_e = e->FirstChildElement("bsdf");
		}
		BSDF* bsdf = loadBSDF(target_e);
		const char* refID;
		target_e->QueryStringAttribute("id", &refID);
		bsdf->refID = refID;
		this->materialIDDictionary[refID] = this->bsdfs.size();
		this->bsdfs.push_back(bsdf);

		std::cout << refID << bsdf->toString() << bsdf->refID << std::endl;
	}

	Diffuse* empty = new Diffuse();
	empty->refID = "empty";
	this->materialIDDictionary["empty"] = this->bsdfs.size();
	this->bsdfs.push_back(empty);
}

void Scene::loadTextures()
{
	HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

	for (BSDF* bsdf : bsdfs) {
		if (bsdf->diffuseReflectanceTexturePath.length() > 0) {
			if (textureIDDictionary.find(bsdf->diffuseReflectanceTexturePath) == textureIDDictionary.end()) {
				textureIDDictionary[bsdf->diffuseReflectanceTexturePath] = textureIDDictionary.size();
				std::cout << "Texture id " << textureIDDictionary.size() << ": "  << bsdf->diffuseReflectanceTexturePath << std::endl;
				
				fs::path fullPath = sceneDirectory / bsdf->diffuseReflectanceTexturePath;
				Texture* texture = new Texture(fullPath.string().c_str());
				this->textures.push_back(texture);
			}
		}
	}
}


void Scene::loadEmitters() 
{
	for (XMLElement* e = root->FirstChildElement("emitter"); e != NULL; e = e->NextSiblingElement("emitter")) {
		const char* shapeType;
		e->QueryStringAttribute("type", &shapeType);
		if (strcmp(shapeType, "envmap") == 0) {
			fs::path fullPath = sceneDirectory / getValueByName(e, "filename");
			this->envMapTexture = new Texture(fullPath.string().c_str());
			this->textures.push_back(this->envMapTexture);
			envMapTransform = loadMatrix4(e->FirstChildElement("transform")->FirstChildElement("matrix"));

		}
	}
}

void Scene::processGUI()
{
	mSceneTotalLoadTimer->processGUI();
}
