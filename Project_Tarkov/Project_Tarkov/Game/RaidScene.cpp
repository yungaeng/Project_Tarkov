#include "RaidScene.h"
#include "RaidResultScene.h"
#include "IndustrialDetails.h"
#include "HideoutMap.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "../Core/Input.h"
#include "../Scene/SceneManager.h"
#include "../Graphics/Renderer.h"
#include <glm/trigonometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <memory>
#include <random>
#include <utility>

RaidScene::RaidScene(SceneManager& manager)
    : sceneManager(manager)
{
}

RaidScene::RaidScene(SceneManager& manager, Hideout carriedHideout)
    : hideout(std::move(carriedHideout)), sceneManager(manager), hasProvidedHideout(true)
{
}

void RaidScene::Init()
{
    std::vector<IndustrialZone::Block> mapBlocks;
    IndustrialZone::Build(collisionWorld, mapBlocks);
    IndustrialZone::AddDetails(collisionWorld, mapBlocks);
    IndustrialZone::Bake(mapBlocks, mapBatches);
    raid = RaidStatus{};
    if (!hasProvidedHideout)
        hideout.Load();
    player = std::make_unique<Player>();
    player->appearance = hideout.appearance;
    player->SetCollisionWorld(&collisionWorld);
    player->position = IndustrialZone::Spawns[0];
    playerController.Reset();
    camera = Camera{};
    freeLook = false;
    firstPerson = false;
    FollowCamera(camera);
    if (!shader.LoadFromFile("Assets/Shaders/cube.vs", "Assets/Shaders/cube.fs"))
        throw std::runtime_error("Shader load failed");
    cubeMesh.CreateCube();
    playerVisual.Init(hideout.appearance);
    loot.Init();
    combat.Init(collisionWorld);
    actions.Cancel();
    focusGeneration = Input::FocusGeneration();
    hud.Init(glfwGetCurrentContext());
    // Opt-in camera for asset inspection in a test save.
    char* setting = nullptr;
    size_t length = 0;
    if (_dupenv_s(&setting, &length, "TARKOV_RAID_PORTRAIT") == 0 && setting) {
        raidPortrait = std::string(setting) == "1";
        std::free(setting);
    }
    EnterHideout();
    if (raidPortrait) StartRaid();
}

void RaidScene::Update(float dt)
{
    if (raid.Finished()) {
        if (raid.phase == RaidPhase::Dead && player) {
            playerVisual.Update(std::clamp(std::isfinite(dt) ? dt : 0.f, 0.f, .1f), *player,
                {true, false, false, false, 0, 0, combat.HasRifle()});
        }
        if (raid.returnToHideout) {
            playerVisual.Reset();
            playerVisual.Init(hideout.appearance);
            raid.phase = RaidPhase::Hideout;
            raid.returnToHideout = false;
            raid.remaining = RaidConfig::RaidDuration;
            raid.extraction = 0.0f;
            raid.assignedExit = 0;
            raid.exitDistance = 0.0f;
            raid.recovered = Inventory{};
            raid.recoveredClothing = 0;
            raid.saveMessage.clear();
            EnterHideout();
        }
        return;
    }
    if (raid.phase == RaidPhase::Hideout) {
        UpdateHideout(dt);
        return;
    }
    const float elapsed = std::isfinite(dt) ? (std::max)(0.0f, dt) : 0.0f;
    raid.remaining = (std::max)(0.0f, raid.remaining - elapsed);
    // Timeout wins over extraction on the same frame.
    if (raid.remaining <= 0) { FinishRaid(RaidPhase::Missing); return; }
    dt = std::clamp(elapsed, 0.0f, 0.1f);
    if (focusGeneration != Input::FocusGeneration()) { actions.Cancel(); combat.CancelInput(); }
    focusGeneration = Input::FocusGeneration();
    bool corpseInteraction = false;
    if (lootingCorpse >= 0 && (closeLootRequested || !Input::IsFocused() ||
        !combat.Corpse(lootingCorpse) || Input::GetKeyDown(GLFW_KEY_ESCAPE) ||
        Input::GetKeyDown(GLFW_KEY_F) || Input::GetKeyDown(GLFW_KEY_I))) {
        CloseCorpseLoot();
        corpseInteraction = true;
    }
    if (lootingCorpse < 0 && !corpseInteraction) playerController.UpdateInterface();
    corpseTarget = playerController.IsGameplayInputEnabled() && lootingCorpse < 0 ?
        combat.FindCorpse(*player, collisionWorld) : -1;
    if (!corpseInteraction && corpseTarget >= 0 && Input::GetKeyDown(GLFW_KEY_F)) {
        corpseInteraction = true;
        const auto* corpse = combat.Corpse(corpseTarget);
        // Keep feet and knees supported while the hands search the corpse's waist.
        const auto placementClear = [&](const glm::vec3& candidate, float height,
            const std::array<glm::vec3, 4>& supports) {
            for (const auto& support : supports)
                if (!collisionWorld.IsSupported(support, .04f, .1f) ||
                    !collisionWorld.CanOccupy(support + glm::vec3(0, .002f, 0), .06f, .15f)) return false;
            if (!collisionWorld.CanOccupy(candidate + glm::vec3(0, .002f, 0), .35f, height)) return false;
            return combat.LootSpaceClear(candidate, corpseTarget);
        };
        const auto anchor = playerVisual.FitLootPose(*player, corpse->body, *corpse->visual, placementClear);
        beforeLootCamera = camera;
        beforeLootPosition = player->position;
        beforeLootRotation = player->rotation.y;
        player->position = anchor;
        player->rotation.y = corpse->body.rotation.y + 180.f;
        player->SetLooting(true);
        lootingCorpse = corpseTarget;
        freeLook = false;
        actions.Cancel();
        combat.CancelInput();
        Input::SetCursorCaptured(false);
    }
    if (lootingCorpse >= 0) {
        if (takeCorpseStack) {
            loot.Notify(combat.TakeCorpseItem(lootingCorpse, takeCorpseStack, *player) ?
                "시체에서 아이템을 획득했습니다." : "휴대 공간이 부족하거나 아이템이 없습니다.");
            takeCorpseStack = 0;
        }
        if (takeCorpseWeapon) {
            loot.Notify(combat.TakeCorpseRifle(lootingCorpse, *player) ? "소총을 획득했습니다." : "무기 슬롯과 휴대 공간이 가득 찼습니다.");
            takeCorpseWeapon = false;
        }
    }
    actions.Update(*player, loot, camera, collisionWorld, (playerController.IsInventoryOpen() || lootingCorpse >= 0) && Input::IsFocused());
    const bool canLook = playerController.IsGameplayInputEnabled() && player->vitals.Alive() && lootingCorpse < 0;
    if (canLook && Input::GetKeyDown(GLFW_KEY_V))
        firstPerson = !firstPerson;
    const bool wantsFreeLook = canLook &&
        (Input::GetKey(GLFW_KEY_LEFT_ALT) || Input::GetKey(GLFW_KEY_RIGHT_ALT));
    if (wantsFreeLook && !freeLook)
        previewCamera = camera;
    freeLook = wantsFreeLook;
    if (canLook)
        cameraController.Rotate(freeLook ? previewCamera : camera, Input::GetMouseDelta());
    if (lootingCorpse < 0) playerController.UpdateMovement(*player, camera);
    else { player->StopMovement(); player->SetSprinting(false); }
    if (canLook &&
        Input::GetKeyDown(GLFW_KEY_T)) {
        player->position = IndustrialZone::Exits[raid.assignedExit].position;
        player->StopMovement();
    }
    if (lootingCorpse < 0) player->Update(dt);
    player->vitals.Update(dt, player->IsSprinting() && player->IsMoving());
    if (player->position.y < -30) player->vitals.Damage(100);
    FollowCamera(camera);
    combat.Update(dt, *player, camera, collisionWorld, loot, canLook);
    if (player->vitals.Alive() && combat.Equipped() && lootingCorpse < 0)
        player->rotation.y = glm::degrees(std::atan2(camera.front.x, camera.front.z));
    FollowCamera(camera);
    loot.Update(dt, *player, camera, collisionWorld, canLook && corpseTarget < 0 && !corpseInteraction, Input::GetKeyDown(GLFW_KEY_F));
    CharacterVisualState visualState{!player->vitals.Alive(), combat.Equipped(), combat.Aiming(),
        combat.Fired(), combat.ReloadTime(), camera.pitch, combat.HasWeapon()};
    visualState.magazineInserted = combat.ActiveWeapon() && combat.ActiveWeapon()->magazineInserted;
    playerVisual.Update(dt, *player, visualState);
    // Resolve against this frame's animated pose, including stance transitions.
    if (firstPerson && lootingCorpse < 0) FollowCamera(camera);
    if (freeLook)
        FollowCamera(previewCamera);
    if (!player->vitals.Alive()) { FinishRaid(RaidPhase::Dead); return; }
    const auto offset = player->position - IndustrialZone::Exits[raid.assignedExit].position;
    raid.exitDistance = glm::length(glm::vec2(offset.x, offset.z));
    if (lootingCorpse < 0 && std::abs(offset.x) <= RaidConfig::ExtractRadius && std::abs(offset.z) <= RaidConfig::ExtractRadius &&
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
        RenderHideoutMap(width, height);
        return;
    }
    const int sceneWidth = width;
    glViewport(0, 0, sceneWidth, height);
    Camera viewCamera = freeLook ? previewCamera : camera;
    if (raidPortrait && raid.phase == RaidPhase::Active) {
        const float yaw = glm::radians(player->rotation.y);
        const glm::vec3 forward(std::sin(yaw), 0, std::cos(yaw));
        const glm::vec3 side(forward.z, 0, -forward.x);
        const auto target = player->position + glm::vec3(0, player->GetEyeHeight() - .06f, 0);
        viewCamera.position = target + forward * .84f + side * .24f + glm::vec3(0,.03f,0);
        viewCamera.front = glm::normalize(target - viewCamera.position);
        viewCamera.up = {0,1,0};
        viewCamera.fieldOfView = 34.f;
    }
    Renderer::PrepareFrame(shader, viewCamera, static_cast<float>(sceneWidth), static_cast<float>(height));
    for (auto& batch : mapBatches)
    {
        const glm::mat4 identity(1);
        if (!Renderer::IsVisible(batch.low, batch.high, identity)) continue;
        Renderer::Draw(shader, batch.mesh, viewCamera, identity,
            static_cast<float>(sceneWidth), static_cast<float>(height), batch.color);
    }
    loot.Render(shader, cubeMesh, viewCamera, static_cast<float>(sceneWidth), static_cast<float>(height));
    playerVisual.Render(*player, shader, viewCamera,
        static_cast<float>(sceneWidth), static_cast<float>(height), glm::vec3(0.3f, 0.8f, 0.4f), firstPerson && lootingCorpse < 0);
    combat.Render(shader, viewCamera, static_cast<float>(sceneWidth), static_cast<float>(height));
    glViewport(0, 0, width, height);
    hud.Render(*player, loot, combat, actions, playerController.IsInventoryOpen(), raid, collisionWorld, shader, combat.EditableCorpse(lootingCorpse), corpseTarget,
        &takeCorpseStack, &takeCorpseWeapon, &closeLootRequested);
}

void RaidScene::FinishRaid(RaidPhase result)
{
    CloseCorpseLoot();
    hud.ResetAmmoIndicator();
    hideout.vitals = player->vitals;
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

        summary = RaidSummary{};
        summary.durationSeconds = static_cast<int>(std::round(RaidConfig::RaidDuration - raid.remaining));
        summary.kills = combat.KillRecords();
        summary.recoveredClothing = player->clothingMask;
        summary.recoveredRifle = combat.HasRifle();
        for (int type = 0; type < ItemTypeCount; ++type) {
            const auto itemType = static_cast<ItemType>(type);
            const auto bit = CanWear(itemType,player->appearance == PlayerAppearance::Male) ? ClothingBit(itemType) : 0u;
            const int startingWorn = (startingClothingMask & bit) ? 1 : 0;
            const int recoveredWorn = (player->clothingMask & bit) ? 1 : 0;
            const int gained = raid.recovered.Count(itemType) + recoveredWorn
                - startingInventory.Count(itemType) - startingWorn;
            if (gained > 0)
                summary.acquiredEquipment.push_back(std::string(GetItemDefinition(itemType).name) + " x" + std::to_string(gained));
        }

        sceneManager.RequestLoad(std::make_unique<RaidResultScene>(sceneManager, std::move(hideout), summary));
        return;
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
    if (!hideout.ready) return;
    if (!hideout.vitals.Alive()) {
        hideout.message = "건강 탭에서 피격 부위를 치료한 뒤 출격하세요.";
        return;
    }
    if (!hideout.Depart()) return;
    std::vector<IndustrialZone::Block> blocks;
    IndustrialZone::Build(collisionWorld, blocks);
    IndustrialZone::AddDetails(collisionWorld, blocks);
    IndustrialZone::Bake(blocks, mapBatches);
    freeLook = false;
    hud.ResetAmmoIndicator();
    raid = RaidStatus{};
    raid.phase = RaidPhase::Active;
    static std::mt19937 randomEngine(std::random_device{}());
    hideout.spawn = std::uniform_int_distribution<int>(0, static_cast<int>(IndustrialZone::Spawns.size()) - 1)(randomEngine);
    raid.assignedExit = hideout.spawn;
    player = std::make_unique<Player>();
    player->appearance = hideout.appearance;
    player->SetCollisionWorld(&collisionWorld);
    player->position = IndustrialZone::Spawns[hideout.spawn];
    player->GetInventory() = hideout.loadout;
    player->clothingMask = hideout.clothingMask;
    player->vitals = hideout.vitals;
    startingInventory = player->GetInventory();
    startingClothingMask = player->clothingMask;
    playerController.Reset();
    camera = Camera{};
    FollowCamera(camera);
    actions.Cancel();
    loot.Init();
    combat.Init(collisionWorld);
    combat.SetLoadout(player->GetInventory());
    summary = RaidSummary{};
    playerVisual.Reset();
    playerVisual.Init(hideout.appearance);
    playerVisual.Update(0, *player, {false, combat.Equipped(), false, false, 0, 0, combat.HasRifle()});
    raid.exitDistance = glm::length(IndustrialZone::Exits[raid.assignedExit].position - player->position);
    focusGeneration = Input::FocusGeneration();
}

void RaidScene::FollowCamera(Camera& target) const
{
    cameraController.Follow(target, *player);
    if (player->IsLooting()) {
        const auto* corpse = combat.Corpse(lootingCorpse);
        if (!corpse) return;
        const float yaw = glm::radians(corpse->body.rotation.y);
        const glm::vec3 forward(std::sin(yaw), 0, std::cos(yaw));
        const glm::vec3 side(forward.z, 0, -forward.x);
        const auto center = (player->position + corpse->body.position) * .5f - forward * .9f + glm::vec3(0, .35f, 0);
        const float direction = glm::dot(player->position - corpse->body.position, side) >= 0 ? 1.f : -1.f;
        target.position = center + side * (3.6f * direction) + forward * .65f + glm::vec3(0, 1.65f, 0);
        target.front = glm::normalize(center - target.position);
        target.up = {0, 1, 0};
        target.fieldOfView = 60.f;
    } else if (firstPerson) {
        target.position = playerVisual.FirstPersonCameraPosition(*player);
    } else {
        target.position.y = (std::max)(target.position.y, 0.25f);
    }
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

void RaidScene::CloseCorpseLoot()
{
    if (lootingCorpse >= 0 && player) {
        player->SetLooting(false);
        camera = beforeLootCamera;
        if (player->vitals.Alive()) {
            player->position = beforeLootPosition;
            player->rotation.y = beforeLootRotation;
        }
        player->StopMovement();
        Input::SetCursorCaptured(Input::IsFocused() && !playerController.IsInventoryOpen());
    }
    lootingCorpse = -1;
    closeLootRequested = false;
    takeCorpseStack = 0;
    takeCorpseWeapon = false;
}

void RaidScene::EnterHideout()
{
    HideoutMap::Build(collisionWorld, mapBatches);
    player = std::make_unique<Player>();
    player->SetCollisionWorld(&collisionWorld);
    player->position = {3,0,2};
    player->appearance = hideout.appearance;
    player->clothingMask = hideout.clothingMask;
    // Allow walking to medical care even when the saved profile needs healing.
    playerVisual.Reset();
    playerVisual.Init(hideout.appearance);
    playerController.Reset();
    firstPerson = false;
    freeLook = false;
    camera = Camera{};
    hideoutPanel = false;
    hideoutFridgePanel = false;
    hideoutExitMenu = false;
    hideoutPrompt = hideoutZone = hideoutStation = 0;
    deployRequested = false;
    playerVisual.Update(0, *player, {false,false,false,false,0,0,false});
    FollowCamera(camera);
}

void RaidScene::UpdateHideout(float dt)
{
    dt = std::clamp(std::isfinite(dt) ? dt : 0.f, 0.f, .1f);
    if (deployRequested) { deployRequested = false; StartRaid(); return; }
    if (hideoutExitMenu) {
        if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            hideoutExitMenu = false;
            Input::SetCursorCaptured(!hideoutPanel&&!hideoutFridgePanel);
        }
        return;
    }
    if (hideoutPanel||hideoutFridgePanel) {
        if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            hideoutExitMenu = true;
            Input::SetCursorCaptured(false);
        } else if (Input::GetKeyDown(GLFW_KEY_F)) {
            hideoutPanel = false;
            hideoutFridgePanel = false;
            player->appearance = hideout.appearance;
            player->clothingMask = hideout.clothingMask;
            Input::SetCursorCaptured(true);
        }
        return;
    }
    if (hideoutPrompt) {
        if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            hideoutPrompt = 0;
            Input::SetCursorCaptured(true);
        }
        return;
    }
    if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
        hideoutExitMenu = true;
        player->StopMovement();
        Input::SetCursorCaptured(false);
        return;
    }
    if (Input::IsFocused()) cameraController.Rotate(camera, Input::GetMouseDelta());
    playerController.UpdateMovement(*player, camera);
    player->Update(dt);
    playerVisual.Update(dt, *player, {false,false,false,false,0,0,false});
    FollowCamera(camera);
    const auto p = player->position;
    int zone = 0;
    if (p.x < -.4f && p.z > .3f) zone = 1;
    else if (std::abs(p.x-9.f)<1.3f && std::abs(p.z)<1.6f) zone = 2;
    if (zone && zone != hideoutZone) {
        hideoutPrompt = zone;
        player->StopMovement();
        Input::SetCursorCaptured(false);
    }
    hideoutZone = zone;
    hideoutStation = 0;
    if (p.x>.2f && p.x<23.8f && p.z < -4.f) {
        hideoutStation = p.x<6.f ? 1 : p.x<12.f ? 2 : p.x<18.f ? 3 : 4;
    }
    if (!hideoutPrompt && hideoutStation && Input::GetKeyDown(GLFW_KEY_F)) {
        hideoutFridgePanel = hideoutStation == 4;
        hideoutPanel = !hideoutFridgePanel;
        if (hideoutPanel) hud.SetHideoutHealthTab(hideoutStation == 1);
        player->StopMovement();
        Input::SetCursorCaptured(false);
    }
}

void RaidScene::RenderHideoutMap(int width, int height)
{
    glViewport(0,0,width,height);
    Renderer::PrepareFrame(shader,camera,float(width),float(height));
    for (auto& batch : mapBatches) {
        if (!Renderer::IsVisible(batch.low,batch.high,glm::mat4(1))) continue;
        Renderer::Draw(shader,batch.mesh,camera,glm::mat4(1),float(width),float(height),batch.color);
    }
    playerVisual.Render(*player,shader,camera,float(width),float(height),glm::vec3(1),false);
    if (hideoutFridgePanel) {
        const bool wasFridgePanelOpen=hideoutFridgePanel;
        hud.RenderHideoutFridge(hideout,hideoutFridgePanel,hideoutExitMenu);
        if (wasFridgePanelOpen&&!hideoutFridgePanel)
            Input::SetCursorCaptured(true);
        return;
    }
    if (hideoutPanel) {
        const bool wasExitMenuOpen=hideoutExitMenu;
        hud.RenderHideout(hideout,*player,playerVisual,shader,true,&hideoutExitMenu);
        if (wasExitMenuOpen && !hideoutExitMenu)
            Input::SetCursorCaptured(false);
        return;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({20,20});
    ImGui::SetNextWindowBgAlpha(.8f);
    ImGui::Begin("은신처 안내",nullptr,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("은신처 · 개방형 테라스 아파트 · 3인칭");
    ImGui::TextUnformatted("복도 왼쪽 끝: 레이드 진입 | 방 1: 의료실 | 방 2: 무기고");
    ImGui::TextUnformatted("방 3: 옷장 | 방 4: 침실 · 물 냉장고");
    ImGui::TextUnformatted("WASD 이동 / 마우스 시점 / F 가구 이용 / Esc 일시 정지 메뉴");
    if (hideoutStation) {
        const char* names[] = {"", "의료 테이블", "무기고", "옷장", "냉장고 · 물 보관"};
        ImGui::Text("[F] %s",names[hideoutStation]);
    }
    if (!hideout.message.empty()) ImGui::TextWrapped("%s",hideout.message.c_str());
    ImGui::End();
    if (hideoutPrompt) {
        ImGui::SetNextWindowPos({width*.5f,height*.5f},ImGuiCond_Always,{.5f,.5f});
        ImGui::Begin("은신처 상호작용",nullptr,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        const auto nextAppearance = hideout.appearance == PlayerAppearance::Female
            ? PlayerAppearance::Male : PlayerAppearance::Female;
        ImGui::TextUnformatted(hideoutPrompt==1 ? "레이드에 진입하시겠습니까?" :
            nextAppearance == PlayerAppearance::Male ? "남캐로 캐릭터 전환 하시겠습니까?" : "여캐로 캐릭터 전환 하시겠습니까?");
        ImGui::BeginDisabled(!Input::IsFocused());
        if(ImGui::Button("예",{120,36})) {
            if(hideoutPrompt==1) deployRequested=true;
            else if(hideout.ChangeAppearance(nextAppearance)) {
                player->appearance=hideout.appearance;
                player->clothingMask=hideout.clothingMask;
                playerVisual.Reset();
                playerVisual.Init(hideout.appearance);
            }
            hideoutPrompt=0;
            Input::SetCursorCaptured(true);
        }
        ImGui::SameLine();
        if(ImGui::Button("아니요",{120,36})) {
            hideoutPrompt=0;
            Input::SetCursorCaptured(true);
        }
        ImGui::EndDisabled();
        ImGui::End();
    }
    const bool wasExitMenuOpen=hideoutExitMenu;
    if (hideoutExitMenu)
        hud.RenderHideoutPauseMenu(hideoutExitMenu);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (wasExitMenuOpen && !hideoutExitMenu)
        Input::SetCursorCaptured(true);
}
