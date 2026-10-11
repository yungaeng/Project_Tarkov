#include "RaidResultScene.h"
#include "RaidScene.h"
#include "../Scene/SceneManager.h"
#include "../Core/Input.h"
#include "../Graphics/Renderer.h"
#include <memory>
#include <stdexcept>
#include <utility>

RaidResultScene::RaidResultScene(SceneManager& manager, Hideout recoveredHideout, RaidSummary raidSummary)
    : sceneManager(manager), hideout(std::move(recoveredHideout)), summary(std::move(raidSummary))
{
}

void RaidResultScene::Init()
{
    character.clothingMask = summary.recoveredClothing;
    character.position = {0, 0, 0};
    if (!shader.LoadFromFile("Assets/Shaders/cube.vs", "Assets/Shaders/cube.fs"))
        throw std::runtime_error("Result scene shader load failed");
    visual.Init();
    visual.Update(0, character, {false, summary.recoveredRifle, false, false, 0, 0, summary.recoveredRifle});
    hud.Init(glfwGetCurrentContext());
    Input::SetCursorCaptured(false);
}

void RaidResultScene::Update(float)
{
    if (!returnRequested)
        return;

    returnRequested = false;
    sceneManager.RequestLoad(std::make_unique<RaidScene>(sceneManager, std::move(hideout)));
}

void RaidResultScene::Render()
{
    Renderer::BeginFrame();
    returnRequested = hud.RenderExtractionSummary(summary, character, visual, shader);
}

void RaidResultScene::Shutdown()
{
    hud.Shutdown();
    visual.Reset();
    shader.Reset();
}
