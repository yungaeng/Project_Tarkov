#include "../../Core/AssetCatalog.h"
#include "RaidResultScene.h"
#include "RaidScene.h"
#include "../../Scene/SceneManager.h"
#include "../../Core/Input.h"
#include "../../Graphics/Renderer.h"
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
    character.rotation.y = 0.0f;
    if (!shader.LoadFromFile(AssetCatalog::Shaders_cube_vs, AssetCatalog::Shaders_cube_fs))
        throw std::runtime_error("Result scene shader load failed");
    visual.Init(hideout.appearance);
    visual.Update(0, character, {false, summary.recoveredRifle, false, false, 0, 0, summary.recoveredRifle});
    if (summary.rescuedFemale) {
        rescuedCharacter.position={0,0,0};
        rescuedCharacter.rotation.y=0.f;
        rescuedCharacter.clothingMask=DefaultClothingMask;
        rescuedVisual.Init(PlayerAppearance::Female);
        rescuedVisual.Update(0,rescuedCharacter,{false,false,false,false,0,0,false});
    }
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
    returnRequested = hud.RenderExtractionSummary(summary,character,visual,shader,
        summary.rescuedFemale?&rescuedCharacter:nullptr,
        summary.rescuedFemale?&rescuedVisual:nullptr);
}

void RaidResultScene::Shutdown()
{
    hud.Shutdown();
    visual.Reset();
    rescuedVisual.Reset();
    shader.Reset();
}
