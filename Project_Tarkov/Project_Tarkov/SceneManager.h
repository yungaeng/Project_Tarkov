#pragma once
#include "pch.h"
#include "Scene.h"
#include <utility>

class SceneManager
{
private:
    std::unique_ptr<Scene> currentScene;

public:
    void Load(std::unique_ptr<Scene> newScene)
    {
        if (currentScene)
        {
            currentScene->Shutdown();
            currentScene.reset();
        }

        currentScene = std::move(newScene);
        if (currentScene) currentScene->Init();
    }

    void Update(float deltaTime)
    {
        if (currentScene)
            currentScene->Update(deltaTime);
    }

    void Render()
    {
        if (currentScene)
            currentScene->Render();
    }

    void Shutdown()
    {
        if (currentScene)
        {
            currentScene->Shutdown();
            currentScene.reset();
        }
    }
};
