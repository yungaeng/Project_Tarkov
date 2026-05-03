#pragma once
#include "pch.h"

class Entity
{
public:
    glm::vec3 position = { 0,0,0 };
    glm::vec3 rotation = { 0,0,0 };
    glm::vec3 scale = { 1,1,1 };

    bool active = true;

    virtual void Update(float dt) {}
    virtual void Render() {}
    virtual ~Entity() {}
};

