#pragma once
#ifndef LOADER_BSDF_H
#define LOADER_BSDF_H
#include "BSDF/BSDF.h"
#include "BSDF/Diffuse.h"
#include "BSDF/Conductor.h"
#include "BSDF/RoughConductor.h"
#include "BSDF/Dielectric.h"
#include "BSDF/RoughDielectric.h"
#include "BSDF/ThinDielectric.h"
#include "BSDF/Plastic.h"
#include "BSDF/RoughPlastic.h"
#include "External/tinyxml2.h"
using namespace tinyxml2;
using namespace std;

BSDF* loadBSDF(XMLElement *e)
{
    const char* bsdfType;
    e->QueryStringAttribute("type", &bsdfType);
    if (strcmp(bsdfType, "twosided") == 0) {
        BSDF* bsdf = loadBSDF(e->FirstChildElement("bsdf"));
        bsdf->isDoubleSided = true;
        return bsdf;
    }
    else if (strcmp(bsdfType, "diffuse") == 0) {
        return new Diffuse(e);
    }
    else if (strcmp(bsdfType, "conductor") == 0) {
        return new Conductor(e);
    }
    else if (strcmp(bsdfType, "roughconductor") == 0) {
        return new RoughConductor(e);
    }
    else if (strcmp(bsdfType, "dielectric") == 0) {
        return new Dielectric(e);
    }
    else if (strcmp(bsdfType, "roughdielectric") == 0) {
        return new RoughDielectric(e);
    }
    else if (strcmp(bsdfType, "thindielectric") == 0) {
        return new Dielectric(e);
    }
    else if (strcmp(bsdfType, "plastic") == 0) {
        return new Plastic(e);
    }
    else if (strcmp(bsdfType, "roughplastic") == 0) {
        return new RoughPlastic(e);
    }
    return new Diffuse();
}
#endif