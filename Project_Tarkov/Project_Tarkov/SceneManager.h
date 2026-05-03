#pragma once
#include "pch.h"
#include "Scene.h"

class SceneManager
{
private:
    Scene* currentScene = nullptr;

public:
    void Load(Scene* newScene)
    {
        if (currentScene)
        {
            currentScene->Shutdown();
            delete currentScene;
        }

        currentScene = newScene;
        currentScene->Init();
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
            delete currentScene;
            currentScene = nullptr;
        }
    }
};
