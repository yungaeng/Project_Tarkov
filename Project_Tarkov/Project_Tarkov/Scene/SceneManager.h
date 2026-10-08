#pragma once
#include <memory>
#include "Scene.h"

class SceneManager
{
private:
    std::unique_ptr<Scene> currentScene;

public:
    void Load(std::unique_ptr<Scene> newScene);

    void Update(float deltaTime);

    void Render();

    void Shutdown();
};
