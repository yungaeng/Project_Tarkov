#pragma once
#include "pch.h"

class Scene
{
public:
    virtual void Init() {}
    virtual void Update() {}
    virtual void Render() {}
    virtual void Shutdown() {}
};

class RaidScene : public Scene
{
private:
    float playerX = 0.0f;

public:
    void Init() override;

    void Update() override;
    

    void Render() override;
   

    void Shutdown() override;
};
