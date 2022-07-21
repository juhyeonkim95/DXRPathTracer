#include "Shape.h"
#include "loader_utils.h"

Shape::Shape(XMLElement* e) 
{
	this->transform = loadMatrix4(e->FirstChildElement("transform")->FirstChildElement("matrix"));
}