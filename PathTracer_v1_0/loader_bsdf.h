#pragma once
#ifndef LOADER_BSDF_H
#define LOADER_BSDF_H
#include "Diffuse.h"
#include "Conductor.h"
#include "RoughConductor.h"
#include "Dielectric.h"
#include "RoughDielectric.h"
#include "ThinDielectric.h"
#include "Plastic.h"
#include "RoughPlastic.h"
#include "tinyxml2.h"
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