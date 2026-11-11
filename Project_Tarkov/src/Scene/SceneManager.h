#pragma once
#include <memory>
#include "Scene.h"

class SceneManager
{
private:
    std::unique_ptr<Scene> currentScene;
    std::unique_ptr<Scene> pendingScene;

public:
    void Load(std::unique_ptr<Scene> newScene);
    void RequestLoad(std::unique_ptr<Scene> newScene);
    void ApplyPending();

    void Update(float deltaTime);

    void Render();

    void Shutdown();
};
