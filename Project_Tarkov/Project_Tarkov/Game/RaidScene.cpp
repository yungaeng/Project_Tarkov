#include "RaidScene.h"
#include "IndustrialDetails.h"
#include "../Core/Input.h"
#include "../Graphics/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>

void RaidScene::Init()
{
    std::vector<IndustrialZone::Block> mapBlocks;
    IndustrialZone::Build(collisionWorld, mapBlocks);
    IndustrialZone::AddDetails(collisionWorld, mapBlocks);
    IndustrialZone::Bake(mapBlocks, mapBatches);
    raid = RaidStatus{};
    hideout.Load();
    player = std::make_unique<Player>();
    player->SetCollisionWorld(&collisionWorld);
    player->position = IndustrialZone::Spawns[0];
    playerController.Reset();
    camera = Camera{};
    freeLook = false;
    cameraController.Follow(camera, *player);
    if (!shader.LoadFromFile("Assets/Shaders/cube.vs", "Assets/Shaders/cube.fs"))
        throw std::runtime_error("Shader load failed");
    cubeMesh.CreateCube();
    playerVisual.Init();
    loot.Init();
    combat.Init(collisionWorld);
    actions.Cancel();
    focusGeneration = Input::FocusGeneration();
    hud.Init(glfwGetCurrentContext());
    Input::SetCursorCaptured(false);
}

void RaidScene::Update(float dt)
{
    if (raid.Finished()) {
        if (raid.returnToHideout) {
            raid.phase = RaidPhase::Hideout;
            raid.returnToHideout = false;
            raid.remaining = RaidConfig::RaidDuration;
            raid.extraction = 0.0f;
            raid.assignedExit = 0;
            raid.exitDistance = 0.0f;
            raid.recovered = Inventory{};
            raid.recoveredClothing = 0;
            raid.saveMessage.clear();
            Input::SetCursorCaptured(false);
        }
        return;
    }
    if (raid.phase == RaidPhase::Hideout) {
        if (deployRequested) { deployRequested = false; StartRaid(); }
        return;
    }
    const float elapsed = std::isfinite(dt) ? (std::max)(0.0f, dt) : 0.0f;
    raid.remaining = (std::max)(0.0f, raid.remaining - elapsed);
    // Timeout wins over extraction on the same frame.
    if (raid.remaining <= 0) { FinishRaid(RaidPhase::Missing); return; }
    dt = std::clamp(elapsed, 0.0f, 0.1f);
    if (focusGeneration != Input::FocusGeneration()) { actions.Cancel(); combat.CancelInput(); }
    focusGeneration = Input::FocusGeneration();
    playerController.UpdateInterface();
    actions.Update(*player, loot, camera, collisionWorld, playerController.IsInventoryOpen() && Input::IsFocused());
    const bool canLook = playerController.IsGameplayInputEnabled() && player->vitals.Alive();
    const bool wantsFreeLook = canLook &&
        (Input::GetKey(GLFW_KEY_LEFT_ALT) || Input::GetKey(GLFW_KEY_RIGHT_ALT));
    if (wantsFreeLook && !freeLook)
        previewCamera = camera;
    freeLook = wantsFreeLook;
    if (canLook)
        cameraController.Rotate(freeLook ? previewCamera : camera, Input::GetMouseDelta());
    playerController.UpdateMovement(*player, camera);
    player->Update(dt);
    player->vitals.Update(dt, player->IsSprinting() && player->IsMoving());
    if (player->position.y < -30) player->vitals.Damage(100);
    cameraController.Follow(camera, *player);
    combat.Update(dt, *player, camera, collisionWorld, loot, playerController.IsGameplayInputEnabled());
    if (player->vitals.Alive() && combat.Equipped())
        player->rotation.y = glm::degrees(std::atan2(camera.front.x, camera.front.z));
    playerVisual.Update(dt, *player, {!player->vitals.Alive(), combat.Equipped(), combat.Aiming(),
        combat.Fired(), combat.ReloadTime(), camera.pitch, combat.HasRifle()});
    cameraController.Follow(camera, *player);
    loot.Update(dt, *player, camera, collisionWorld, playerController.IsGameplayInputEnabled() && player->vitals.Alive(), Input::GetKeyDown(GLFW_KEY_F));
    if (freeLook)
        cameraController.Follow(previewCamera, *player);
    if (!player->vitals.Alive()) { FinishRaid(RaidPhase::Dead); return; }
    const auto offset = player->position - IndustrialZone::Exits[raid.assignedExit].position;
    raid.exitDistance = glm::length(glm::vec2(offset.x, offset.z));
    if (std::abs(offset.x) <= RaidConfig::ExtractRadius && std::abs(offset.z) <= RaidConfig::ExtractRadius &&
        std::abs(offset.y) < 2.0f)
        raid.extraction += elapsed;
    else raid.extraction = 0;
    if (raid.extraction >= RaidConfig::DefaultExtractTime) FinishRaid(RaidPhase::Extracted);
}

void RaidScene::Render()
{
    shader.HotReload();
    Renderer::BeginFrame();
    int width = 0, height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0) return;
    if (raid.phase == RaidPhase::Hideout) {
        deployRequested = hud.RenderHideout(hideout,*player,playerVisual,shader);
        return;
    }
    Camera& viewCamera = freeLook ? previewCamera : camera;
    Renderer::PrepareFrame(shader, viewCamera, static_cast<float>(width), static_cast<float>(height));
    for (auto& batch : mapBatches)
    {
        const glm::mat4 identity(1);
        if (!Renderer::IsVisible(batch.low, batch.high, identity)) continue;
        Renderer::Draw(shader, batch.mesh, viewCamera, identity,
            static_cast<float>(width), static_cast<float>(height), batch.color);
    }
    loot.Render(shader, cubeMesh, viewCamera, static_cast<float>(width), static_cast<float>(height));
    playerVisual.Render(*player, shader, viewCamera,
        static_cast<float>(width), static_cast<float>(height));
    combat.Render(shader, viewCamera, static_cast<float>(width), static_cast<float>(height));
    hud.Render(*player, loot, combat, actions, playerController.IsInventoryOpen(), raid);
}

void RaidScene::FinishRaid(RaidPhase result)
{
    freeLook = false;
    raid.phase = result;
    raid.returnToHideout = false;
    actions.Cancel();
    combat.CancelInput();
    player->StopMovement();
    Input::SetCursorCaptured(false);
    if (result == RaidPhase::Extracted) {
        raid.recovered = player->GetInventory();
        raid.recoveredClothing = player->clothingMask;
        hideout.Recover(raid.recovered, combat.HasRifle(), combat.Magazine(), player->clothingMask);
        raid.saveMessage = "탈출 성공: 보급품을 은신처로 반입했습니다.";
    }
    else if (result == RaidPhase::Dead) {
        player->GetInventory() = Inventory{};
        hideout.Recover(Inventory{}, false, 0, 0);
        raid.saveMessage = "전사 처리: 장비와 소지품이 모두 손실되었습니다.";
    }
    else {
        player->GetInventory() = Inventory{};
        hideout.Recover(Inventory{}, false, 0, 0);
        raid.saveMessage = "타이머 종료: MIA 처리되어 장비를 잃었습니다.";
    }

    if (!hideout.message.empty())
        raid.saveMessage = hideout.message;
}

void RaidScene::StartRaid()
{
    if (!hideout.ready || !hideout.Depart()) return;
    freeLook = false;
    raid = RaidStatus{};
    raid.phase = RaidPhase::Active;
    raid.assignedExit = hideout.spawn;
    player = std::make_unique<Player>();
    player->SetCollisionWorld(&collisionWorld);
    player->position = IndustrialZone::Spawns[hideout.spawn];
    player->GetInventory() = hideout.loadout;
    player->clothingMask = hideout.clothingMask;
    playerController.Reset();
    camera = Camera{};
    cameraController.Follow(camera, *player);
    actions.Cancel();
    loot.Init();
    combat.Init(collisionWorld);
    combat.SetLoadout(hideout.rifle, hideout.magazine);
    playerVisual.Reset();
    playerVisual.Init();
    playerVisual.Update(0, *player, {false, combat.Equipped(), false, false, 0, 0, combat.HasRifle()});
    raid.exitDistance = glm::length(IndustrialZone::Exits[raid.assignedExit].position - player->position);
    focusGeneration = Input::FocusGeneration();
}

void RaidScene::Shutdown()
{
    actions.Cancel();
    hud.Shutdown();
    combat.Reset();
    loot.Reset();
    player.reset();
    playerVisual.Reset();
    mapBatches.clear();
    collisionWorld.Clear();
    cubeMesh.Reset();
    shader.Reset();
}
