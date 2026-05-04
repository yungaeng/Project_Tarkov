#pragma once
#include "Mesh.h"

class ModelLoader
{
public:
    static Mesh LoadFBX(
        const std::string& path);
};