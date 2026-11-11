#pragma once
#include "Mesh.h"
#include <string>

struct LoadedModel
{
    Mesh mesh;
    std::string diffuseTexturePath;
};

class ModelLoader
{
public:
    static LoadedModel LoadFBX(
        const std::string& path);
};
