#pragma once
#include "pch.h"

class Scene
{
public:
    virtual void Init() {}
    virtual void Update(float dt) {}
    virtual void Render() {}
    virtual void Shutdown() {}

    virtual ~Scene() {}
};


