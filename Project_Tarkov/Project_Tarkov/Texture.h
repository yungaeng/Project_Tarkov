#pragma once
#include "pch.h"

class Texture
{
public:
    GLuint id = 0;

    bool Load(
        const char* path);

    void Bind(
        int slot = 0);
};

