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

void SceneManager::RequestLoad(std::unique_ptr<Scene> newScene)
{
    pendingScene = std::move(newScene);
}

void SceneManager::ApplyPending()
{
    if (!pendingScene)
        return;

    auto nextScene = std::move(pendingScene);
    Load(std::move(nextScene));
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
    pendingScene.reset();
    if (currentScene)
    {
        currentScene->Shutdown();
        currentScene.reset();
    }
}
