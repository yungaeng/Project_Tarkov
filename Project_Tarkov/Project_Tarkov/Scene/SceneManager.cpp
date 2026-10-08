#include "SceneManager.h"
#include <utility>

void SceneManager::Load(std::unique_ptr<Scene> newScene)
{
    if (currentScene)
    {
        currentScene->Shutdown();
        currentScene.reset();
    }

    currentScene = std::move(newScene);
    if (currentScene) currentScene->Init();
}

void SceneManager::Update(float deltaTime)
{
    if (currentScene)
        currentScene->Update(deltaTime);
}

void SceneManager::Render()
{
    if (currentScene)
        currentScene->Render();
}

void SceneManager::Shutdown()
{
    if (currentScene)
    {
        currentScene->Shutdown();
        currentScene.reset();
    }
}
