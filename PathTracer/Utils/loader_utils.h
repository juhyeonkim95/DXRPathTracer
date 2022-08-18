#pragma once
#ifndef LOADER_UTILS_H
#define LOADER_UTILS_H
#include <sstream>
#include <vector>
#include "External/tinyxml2.h"
#include "Framework.h"
using namespace tinyxml2;
using namespace std;


void tokenize(string const& str, const char delim, std::vector<string>& out);
XMLElement* findElementByName(XMLElement* e, std::string const& targetName);
std::string getValueByName(XMLElement* e, std::string const& targetName);

int getIntByName(XMLElement* e, std::string const& targetName);
vec3 getVec3ByName(XMLElement* e, std::string const& targetName, vec3 defaultValue);
float getFloatByName(XMLElement* e, std::string const& targetName, float defaultValue);
bool getBoolByName(XMLElement* e, std::string const& targetName, bool defaultValue);

std::string getValueByNameDefault(XMLElement* e, std::string const& targetName, std::string defaultValue);

vec3 loadVec3(XMLElement* e);
mat4 loadMatrix4(XMLElement* e);

#endif