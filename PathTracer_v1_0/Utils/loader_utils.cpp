#include "loader_utils.h"
#include <iostream>

void tokenize(string const& str, const char delim, std::vector<string>& out) {
    out.clear();
    std::stringstream ss(str);
    string s;
    while (std::getline(ss, s, delim)) {
        out.push_back(s);
    }
}

XMLElement* findElementByName(XMLElement* e, std::string const &targetName) 
{
    XMLElement* child = e->FirstChildElement();
    while (child) {
        const char* elementName;
        child->QueryStringAttribute("name", &elementName);
        if (targetName.compare(elementName) == 0) {
            return child;
        }
        child = child->NextSiblingElement();
    }
    return (NULL);
}

std::string getValueByName(XMLElement* e, std::string const &targetName)
{
    XMLElement* child = findElementByName(e, targetName);
    const char* value;
    child->QueryStringAttribute("value", &value);
    return value;
}

std::string getValueByNameDefault(XMLElement* e, std::string const& targetName, std::string defaultValue)
{
    XMLElement* child = findElementByName(e, targetName);
    if (child == NULL) {
        return defaultValue;
    }
    const char* value;
    child->QueryStringAttribute("value", &value);
    return value;
}


int getIntByName(XMLElement* e, std::string const& targetName)
{
    XMLElement* child = findElementByName(e, targetName);
    int value;
    child->QueryIntAttribute("value", &value);
    return value;
}

vec3 getVec3ByName(XMLElement* e, std::string const& targetName, vec3 defaultValue)
{
    XMLElement* child = findElementByName(e, targetName);
    if (child == NULL || (strcmp(child->Value(), "texture") == 0)) {
        return defaultValue;
    }
    return loadVec3(child);
}

float getFloatByName(XMLElement* e, std::string const& targetName, float defaultValue)
{
    XMLElement* child = findElementByName(e, targetName);
    if (child == NULL) {
        return defaultValue;
    }
    float value;
    child->QueryFloatAttribute("value", &value);
    return value;
}

vec3 loadVec3(XMLElement* e)
{
    const char* valueString;
    e->QueryStringAttribute("value", &valueString);
    //string valueString2 = valueString;
    //valueString2.erase(std::remove(valueString2.begin(), valueString2.end(), ' '), valueString2.end());
    std::vector<string> stringElements;
    tokenize(valueString, ',', stringElements);
    vec3 vector;
    vector.x = atof(stringElements[0].c_str());
    vector.y = atof(stringElements[1].c_str());
    vector.z = atof(stringElements[2].c_str());

    return vector;
}

mat4 loadMatrix4(XMLElement* e)
{
    const char* matrixString;
    e->QueryStringAttribute("value", &matrixString);
    std::vector<string> maxtrixElements;
    tokenize(matrixString, ' ', maxtrixElements);
    mat4 matrix;
    matrix[0][0] = atof(maxtrixElements.at(0).c_str());
    matrix[0][1] = atof(maxtrixElements.at(1).c_str());
    matrix[0][2] = atof(maxtrixElements.at(2).c_str());
    matrix[0][3] = atof(maxtrixElements.at(3).c_str());
    matrix[1][0] = atof(maxtrixElements.at(4).c_str());
    matrix[1][1] = atof(maxtrixElements.at(5).c_str());
    matrix[1][2] = atof(maxtrixElements.at(6).c_str());
    matrix[1][3] = atof(maxtrixElements.at(7).c_str());
    matrix[2][0] = atof(maxtrixElements.at(8).c_str());
    matrix[2][1] = atof(maxtrixElements.at(9).c_str());
    matrix[2][2] = atof(maxtrixElements.at(10).c_str());
    matrix[2][3] = atof(maxtrixElements.at(11).c_str());
    matrix[3][0] = atof(maxtrixElements.at(12).c_str());
    matrix[3][1] = atof(maxtrixElements.at(13).c_str());
    matrix[3][2] = atof(maxtrixElements.at(14).c_str());
    matrix[3][3] = atof(maxtrixElements.at(15).c_str());

    return transpose(matrix);
}