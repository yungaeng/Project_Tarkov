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
    if (rescueRequested) {
        rescueRequested=false;
        if (combat.RecruitRescue()) {
            loot.Notify("여성이 동행합니다. 적의 공격으로부터 보호하며 함께 탈출하세요.");
            Input::SetCursorCaptured(true);
        }
    }
    if (rescuePromptOpen) {
        if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            rescuePromptOpen=false;
            Input::SetCursorCaptured(true);
        } else {
            player->StopMovement();
            return;
        }
    }
    const float elapsed = std::isfinite(dt) ? (std::max)(0.0f, dt) : 0.0f;
    raid.remaining = (std::max)(0.0f, raid.remaining - elapsed);
    // Timeout wins over extraction on the same frame.
    if (raid.remaining <= 0) { FinishRaid(RaidPhase::Missing); return; }
    dt = std::clamp(elapsed, 0.0f, 0.1f);
    if (focusGeneration != Input::FocusGeneration()) { actions.Cancel(); combat.CancelInput(); }
    focusGeneration = Input::FocusGeneration();
    bool corpseInteraction = false;
    if (lootingCorpse>=0 && !combat.Corpse(lootingCorpse)) {
        CloseCorpseLoot();
        corpseInteraction=true;
    } else if (lootingCorpse>=0 && corpseLootPanelOpen &&
        (closeLootRequested || !Input::IsFocused() || Input::GetKeyDown(GLFW_KEY_ESCAPE) ||
         Input::GetKeyDown(GLFW_KEY_F) || Input::GetKeyDown(GLFW_KEY_I))) {
        CloseCorpseLoot();
        corpseInteraction=true;
    }
    if (lootingCorpse < 0 && !corpseInteraction) playerController.UpdateInterface();
    corpseTarget = playerController.IsGameplayInputEnabled() && lootingCorpse < 0 ?
        combat.FindCorpse(*player, collisionWorld) : -1;
    if (lootingCorpse<0 && playerController.IsGameplayInputEnabled() && !corpseInteraction &&
        lootingCorpse<0 && corpseTarget<0 &&
        Input::GetKeyDown(GLFW_KEY_F) && combat.CanInteractWithRescue(*player,collisionWorld)) {
        rescuePromptOpen=true;
        player->StopMovement();
        actions.Cancel();
        combat.CancelInput();
        Input::SetCursorCaptured(false);
    }
    auto beginCorpseLoot=[&]() {
        const auto* corpse=combat.Corpse(corpseTarget);
        if (!corpse) return;
        corpseInteraction=true;
        corpse->visual->SetLootCorpseFlipped(false,corpse->body);
        beforeLootCamera = camera;
        beforeLootPosition = player->position;
        beforeLootRotation = player->rotation.y;
        const auto placementClear = [&](const glm::vec3& candidate, float height,
            const std::array<glm::vec3, 4>& supports) {
            for (const auto& support : supports)
                if (!collisionWorld.IsSupported(support, .04f, .1f) ||
                    !collisionWorld.CanOccupy(support + glm::vec3(0, .002f, 0), .06f, .15f)) return false;
            if (!collisionWorld.CanOccupy(candidate + glm::vec3(0, .002f, 0), .35f, height)) return false;
            return combat.LootSpaceClear(candidate, corpseTarget);
        };
        const auto anchor = playerVisual.FitLootPose(*player, corpse->body, *corpse->visual, placementClear);
        player->position = anchor;
        player->rotation.y = playerVisual.FittedLootYaw();
        player->SetLooting(true);
        lootingCorpse = corpseTarget;
        corpseLootPanelOpen=true;
        freeLook = false;
        actions.Cancel();
        combat.CancelInput();
        Input::SetCursorCaptured(false);
    };
    if (!corpseInteraction && lootingCorpse<0 && corpseTarget>=0 &&
        Input::GetKeyDown(GLFW_KEY_F)) {
        beginCorpseLoot();
    }
    if (lootingCorpse >= 0 && corpseLootPanelOpen) {
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
    actions.Update(*player,loot,camera,collisionWorld,
        (playerController.IsInventoryOpen()||corpseLootPanelOpen)&&Input::IsFocused());
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
    loot.Update(dt,*player,camera,collisionWorld,
        canLook&&corpseTarget<0&&!corpseInteraction&&!rescuePromptOpen&&lootingCorpse<0,
        Input::GetKeyDown(GLFW_KEY_F)&&!rescuePromptOpen);
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
    const auto* rescuedBody=combat.RescueBody();
    const bool rescuedAtExit=!combat.HasRecruitedRescue() ||
        (rescuedBody &&
         std::abs(rescuedBody->position.x-IndustrialZone::Exits[raid.assignedExit].position.x)<=RaidConfig::ExtractRadius &&
         std::abs(rescuedBody->position.z-IndustrialZone::Exits[raid.assignedExit].position.z)<=RaidConfig::ExtractRadius &&
         std::abs(rescuedBody->position.y)<2.f);
    if (lootingCorpse < 0 && rescuedAtExit &&
        std::abs(offset.x) <= RaidConfig::ExtractRadius && std::abs(offset.z) <= RaidConfig::ExtractRadius &&
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
    const int corpsePromptTarget=corpseLootPanelOpen?-1:
        lootingCorpse>=0?lootingCorpse:corpseTarget;
    hud.Render(*player,loot,combat,actions,playerController.IsInventoryOpen(),raid,
        collisionWorld,shader,corpseLootPanelOpen?combat.EditableCorpse(lootingCorpse):nullptr,
        corpsePromptTarget,
        &takeCorpseStack, &takeCorpseWeapon, &closeLootRequested,&rescuePromptOpen,&rescueRequested);
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
        const bool rescueAttempted=combat.HasRecruitedRescue();
        const int rescuedBefore=hideout.rescuedSurvivors;
        hideout.Recover(raid.recovered,combat.HasRifle(),combat.Magazine(),
            player->clothingMask,rescueAttempted);
        const bool rescuedFemale=rescueAttempted&&hideout.rescuedSurvivors>rescuedBefore;
        raid.saveMessage = "탈출 성공: 보급품을 은신처로 반입했습니다.";

        summary = RaidSummary{};
        summary.durationSeconds = static_cast<int>(std::round(RaidConfig::RaidDuration - raid.remaining));
        summary.kills = combat.KillRecords();
        summary.recoveredClothing = player->clothingMask;
        summary.recoveredRifle = combat.HasRifle();
        summary.rescuedFemale=rescuedFemale;
        if (rescuedFemale) summary.acquiredEquipment.push_back("여캐 (합류함)");
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
    rescuePromptOpen=false;
    rescueRequested=false;
    corpseLootPanelOpen=false;
    lootingCorpse=-1;
    corpseTarget=-1;
    if (hideout.rescuedSurvivors<Hideout::MaxRescuedSurvivors)
        combat.SpawnRescueSurvivor(player->position,collisionWorld);
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
    corpseLootPanelOpen=false;
    closeLootRequested = false;
    takeCorpseStack = 0;
    takeCorpseWeapon = false;
}

void RaidScene::EnterHideout()
{
    HideoutMap::Build(collisionWorld,mapBatches,
        hideout.AvailableRoomCount(),hideout.rescuedRoomMask);
    player = std::make_unique<Player>();
    player->SetCollisionWorld(&collisionWorld);
    player->position = {3,0,2};
    player->appearance = hideout.appearance;
    player->clothingMask = hideout.clothingMask;
    // Allow walking to medical care even when the saved profile needs healing.
    playerVisual.Reset();
    playerVisual.Init(hideout.appearance);
    ConfigureHideoutSwitchNpc();
    playerController.Reset();
    firstPerson = false;
    freeLook = false;
    camera = Camera{};
    hideoutPanel = false;
    hideoutStoragePanel = false;
    hideoutExitMenu = false;
    hideoutPrompt = hideoutZone = 0;
    hideoutPromptRoom = -1;
    hideoutBedPoseActive=false;
    hideoutBedTestPrompt=false;
    corpseLootPanelOpen=false;
    hideoutFurniture = -1;
    hideoutCameraInRoom = false;
    hideoutCameraModelHidden = false;
    hideoutCameraBlend = 0.f;
    deployRequested = false;
    playerVisual.Update(0, *player, {false,false,false,false,0,0,false});
    FollowCamera(camera);
}

void RaidScene::ConfigureHideoutSwitchNpc()
{
    hideoutSwitchNpcAppearance = hideout.appearance == PlayerAppearance::Female
        ? PlayerAppearance::Male : PlayerAppearance::Female;
    hideoutSwitchNpc.position = {
        hideoutSwitchNpcAppearance == PlayerAppearance::Female ? 9.f : 3.f, 0.f, -4.5f
    };
    hideoutSwitchNpcVisual.Reset();
    hideoutSwitchNpcVisual.Init(hideoutSwitchNpcAppearance);
    CharacterVisualState npcState;
    npcState.hasWeapon = false;
    hideoutSwitchNpcVisual.Update(0, hideoutSwitchNpc, npcState);
    hideoutResidentNpcs.clear();
    hideoutResidentVisuals.clear();
    hideoutResidentRooms.clear();
    for (int room=Hideout::InitialRoomCount;room<Hideout::HideoutRoomCount;++room) {
        if (!hideout.HasRescuedSurvivor(room)) continue;
        auto resident=std::make_unique<Character>();
        resident->position={room*6.f+3.f,0.f,-4.5f};
        auto visual=std::make_unique<CharacterVisual>();
        visual->Init(PlayerAppearance::Female);
        visual->Update(0,*resident,npcState);
        hideoutResidentNpcs.push_back(std::move(resident));
        hideoutResidentVisuals.push_back(std::move(visual));
        hideoutResidentRooms.push_back(room);
    }
}

void RaidScene::BeginHideoutBedPoseTest(int room)
{
    if (!player || room<0 || room>=Hideout::InitialRoomCount) return;
    hideoutBedBeforePlayerPosition=player->position;
    hideoutBedBeforePlayerRotation=player->rotation.y;
    hideoutBedBeforeNpcPosition=hideoutSwitchNpc.position;
    hideoutBedBeforeNpcRotation=hideoutSwitchNpc.rotation.y;
    hideoutBedBeforeCamera=camera;
    hideoutBedTestRoom=room;
    const glm::vec3 bedPosition(room*6.f+1.1f,.8f,-4.8f);
    hideoutBedNpcMoved=true;
    if (hideoutBedNpcMoved) {
        hideoutSwitchNpc.position=bedPosition;
        hideoutSwitchNpc.rotation.y=hideoutSwitchNpcAppearance==PlayerAppearance::Male?90.f:0.f;
        CharacterVisualState npcState;
        npcState.dead=true;
        hideoutSwitchNpcVisual.Update(0,hideoutSwitchNpc,npcState);
    }

    playerVisual.SetLootPreview(false);
    playerVisual.SetHideoutKneelPose(false);
    player->SetLooting(false);
    playerVisual.Update(0,*player,{false,false,false,false,0,0,false});
    hideoutBedPoseElapsed=0.f;
    hideoutBedPoseLooting=false;
    hideoutBedCorpseAligned=false;
    hideoutCameraInRoom=false;
    hideoutCameraModelHidden=false;
    hideoutCameraBlend=0.f;
    hideoutBedPoseActive=true;
    hideoutBedTestPrompt=false;
    Input::SetCursorCaptured(true);
}

void RaidScene::BeginHideoutBedLooting()
{
    const glm::vec3 bedPosition(hideoutBedTestRoom*6.f+1.1f,.8f,-4.8f);
    const auto placementClear=[&](const glm::vec3& candidate,float,
        const std::array<glm::vec3,4>& supports) {
        const float bedLeft=bedPosition.x-.69f,bedRight=bedPosition.x+.69f;
        const float bedFront=bedPosition.z-1.03f,bedBack=bedPosition.z+1.03f;
        if (candidate.x<bedLeft||candidate.x>bedRight||
            candidate.z<bedFront||candidate.z>bedBack) return false;
        for (const auto& support:supports)
            if (support.x<bedLeft||support.x>bedRight||
                support.z<bedFront||support.z>bedBack) return false;
        return true;
    };
    const auto lootAnchor=playerVisual.FitLootPose(*player,hideoutSwitchNpc,
        hideoutSwitchNpcVisual,placementClear,90.f,.8f);
    player->position=lootAnchor;
    if (!playerVisual.LootPoseValid()) {
        const auto& supports=playerVisual.LootSupports();
        float minX=supports[0].x-lootAnchor.x, maxX=minX;
        float minZ=supports[0].z-lootAnchor.z, maxZ=minZ;
        for (const auto& support:supports) {
            minX=(std::min)(minX,support.x-lootAnchor.x);
            maxX=(std::max)(maxX,support.x-lootAnchor.x);
            minZ=(std::min)(minZ,support.z-lootAnchor.z);
            maxZ=(std::max)(maxZ,support.z-lootAnchor.z);
        }
        const float left=bedPosition.x-.69f+.04f, right=bedPosition.x+.69f-.04f;
        const float front=bedPosition.z-1.03f+.04f, back=bedPosition.z+1.03f-.04f;
        const float shiftX=std::clamp(bedPosition.x-(minX+maxX)*.5f,
            left-minX,right-maxX);
        const float shiftZ=std::clamp(bedPosition.z-(minZ+maxZ)*.5f,
            front-minZ,back-maxZ);
        player->position.x+=shiftX;
        player->position.z+=shiftZ;
    }
    player->rotation.y=playerVisual.FittedLootYaw();
    player->SetLooting(true);
    playerVisual.Update(0,*player,{false,false,false,false,0,0,false});
    playerVisual.AlignLootPosePelvis(*player,
        {bedPosition.x-.15f,.62f,bedPosition.z+.43f});
    hideoutBedPoseLooting=true;
}

void RaidScene::EndHideoutBedPoseTest()
{
    if (!hideoutBedPoseActive || !player) return;
    player->SetLooting(false);
    player->position=hideoutBedBeforePlayerPosition;
    player->rotation.y=hideoutBedBeforePlayerRotation;
    if (hideoutBedNpcMoved) {
        hideoutSwitchNpc.position=hideoutBedBeforeNpcPosition;
        hideoutSwitchNpc.rotation.y=hideoutBedBeforeNpcRotation;
        hideoutSwitchNpcVisual.Reset();
        hideoutSwitchNpcVisual.Init(hideoutSwitchNpcAppearance);
        CharacterVisualState npcState;
        hideoutSwitchNpcVisual.Update(0,hideoutSwitchNpc,npcState);
    }
    hideoutBedNpcMoved=false;
    hideoutBedTestRoom=-1;
    hideoutBedPoseElapsed=0.f;
    hideoutBedPoseLooting=false;
    hideoutBedCorpseAligned=false;
    playerVisual.SetLootPreview(false);
    playerVisual.SetHideoutKneelPose(false);
    hideoutBedPoseActive=false;
    hideoutBedTestPrompt=false;
    hideoutCameraInRoom=false;
    hideoutCameraModelHidden=false;
    hideoutCameraBlend=0.f;
    camera=hideoutBedBeforeCamera;
    Input::SetCursorCaptured(true);
}

void RaidScene::UpdateHideout(float dt)
{
    dt = std::clamp(std::isfinite(dt) ? dt : 0.f, 0.f, .1f);
    if (hideoutBedPoseActive) {
        if (Input::GetKeyDown(GLFW_KEY_F)||Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            EndHideoutBedPoseTest();
            return;
        }
        player->StopMovement();
        hideoutBedPoseElapsed+=dt;
        CharacterVisualState npcState;
        npcState.dead=true;
        hideoutSwitchNpcVisual.Update(dt,hideoutSwitchNpc,npcState);
        if (!hideoutBedCorpseAligned&&hideoutBedPoseElapsed>=1.3f) {
            hideoutSwitchNpcVisual.AlignDeathPosePelvis(hideoutSwitchNpc,
                {hideoutBedTestRoom*6.f+.72f,.62f,-4.37f});
            hideoutBedCorpseAligned=true;
        }
        if (!hideoutBedPoseLooting&&hideoutBedPoseElapsed>=1.3f)
            BeginHideoutBedLooting();
        playerVisual.Update(dt,*player,{false,false,false,false,0,0,false});
        const glm::vec3 center(hideoutBedTestRoom*6.f+1.1f,.9f,-4.8f);
        camera.position=center+glm::vec3(2.8f,3.2f,4.2f);
        camera.front=glm::normalize(center-camera.position);
        camera.up={0,1,0};
        camera.fieldOfView=48.f;
        return;
    }
    if (deployRequested) { deployRequested = false; StartRaid(); return; }
    if (hideoutExitMenu) {
        if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            hideoutExitMenu = false;
            Input::SetCursorCaptured(!hideoutPanel&&!hideoutStoragePanel);
        }
        return;
    }
    if (hideoutPanel||hideoutStoragePanel) {
        if (Input::GetKeyDown(GLFW_KEY_ESCAPE)) {
            hideoutExitMenu = true;
            Input::SetCursorCaptured(false);
        } else if (Input::GetKeyDown(GLFW_KEY_F)) {
            hideoutPanel = false;
            hideoutStoragePanel = false;
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
    if (Input::GetKeyDown(GLFW_KEY_Y) &&
        hideout.HasRescuedSurvivor(Hideout::InitialRoomCount) &&
        hideout.ChangeAppearance(hideout.appearance,Hideout::InitialRoomCount)) {
        HideoutMap::Build(collisionWorld,mapBatches,
            hideout.AvailableRoomCount(),hideout.rescuedRoomMask);
        ConfigureHideoutSwitchNpc();
        return;
    }
    if (Input::IsFocused()) cameraController.Rotate(camera, Input::GetMouseDelta());
    playerController.UpdateMovement(*player, camera);
    player->Update(dt);
    playerVisual.Update(dt, *player, {false,false,false,false,0,0,false});
    CharacterVisualState npcState;
    npcState.hasWeapon = false;
    hideoutSwitchNpcVisual.Update(dt, hideoutSwitchNpc, npcState);
    for (std::size_t i=0;i<hideoutResidentNpcs.size();++i)
        hideoutResidentVisuals[i]->Update(dt,*hideoutResidentNpcs[i],npcState);
    const auto p = player->position;
    const float roomRight=hideout.AvailableRoomCount()*6.f;
    if (!hideoutCameraInRoom && p.x > .2f && p.x < roomRight-.2f && p.z < -.65f && p.z > -7.5f)
        hideoutCameraInRoom = true;
    else if (hideoutCameraInRoom &&
        (p.x < -.2f || p.x > roomRight+.2f || p.z > .15f || p.z < -7.8f))
        hideoutCameraInRoom = false;
    const float cameraTarget = hideoutCameraInRoom ? 1.f : 0.f;
    hideoutCameraBlend += std::clamp(cameraTarget - hideoutCameraBlend,
        -dt / .7f, dt / .7f);
    const float cameraEase = hideoutCameraBlend * hideoutCameraBlend *
        (3.f - 2.f * hideoutCameraBlend);
    const auto previousCameraPosition = camera.position;
    cameraController.Follow(camera, *player);
    const auto thirdPersonCameraPosition = camera.position;
    const auto desiredCameraPosition = glm::mix(thirdPersonCameraPosition,
        playerVisual.FirstPersonCameraPosition(*player), cameraEase);
    if (hideoutCameraBlend == 0.f)
        camera.position = thirdPersonCameraPosition;
    else
        camera.position = glm::mix(previousCameraPosition, desiredCameraPosition,
            1.f - std::exp(-dt * 10.f));
    if (!hideoutCameraModelHidden && hideoutCameraBlend >= .98f)
        hideoutCameraModelHidden = true;
    else if (hideoutCameraModelHidden && hideoutCameraBlend <= .02f)
        hideoutCameraModelHidden = false;
    int zone = 0;
    if (p.x < -.4f && p.z > .3f) zone = 1;
    if (zone && zone != hideoutZone) {
        hideoutPrompt = zone;
        player->StopMovement();
        Input::SetCursorCaptured(false);
    }
    hideoutZone = zone;
    float nearestNpcDistance=2.25f;
    bool nearSwitchNpc=false;
    PlayerAppearance interactionAppearance=hideoutSwitchNpcAppearance;
    int interactionRoom=-1;
    const float npcDx=p.x-hideoutSwitchNpc.position.x,npcDz=p.z-hideoutSwitchNpc.position.z;
    const float baseNpcDistance=npcDx*npcDx+npcDz*npcDz;
    if (baseNpcDistance<nearestNpcDistance) {
        nearestNpcDistance=baseNpcDistance;
        nearSwitchNpc=true;
    }
    for (std::size_t i=0;i<hideoutResidentNpcs.size();++i) {
        const auto& resident=hideoutResidentNpcs[i];
        const float dx=p.x-resident->position.x,dz=p.z-resident->position.z;
        const float distance=dx*dx+dz*dz;
        if (distance<nearestNpcDistance) {
            nearestNpcDistance=distance;
            nearSwitchNpc=true;
            interactionAppearance=PlayerAppearance::Female;
            interactionRoom=hideoutResidentRooms[i];
        }
    }
    hideoutFurniture = -1;
    float nearestFurniture = 1.7f * 1.7f;
    if (!nearSwitchNpc && p.x>.2f && p.x<roomRight && p.z < .25f && p.z > -8.f) {
        hideoutFurnitureRoom = (std::min)(Hideout::HideoutRoomCount-1,
            static_cast<int>(p.x/6.f));
        const float roomX=hideoutFurnitureRoom*6.f;
        const float furnitureX[]{
            roomX+5.1f, roomX+2.8f, roomX+4.9f, roomX+1.2f
        };
        const float furnitureZ[]{-4.8f,-7.f,-6.8f,-2.5f};
        for (int kind=0;kind<Hideout::FurnitureCount;++kind) {
            const float dx=p.x-furnitureX[kind],dz=p.z-furnitureZ[kind];
            const float distance=dx*dx+dz*dz;
            if (distance<nearestFurniture) {
                nearestFurniture=distance;
                hideoutFurniture=kind;
            }
        }
    }
    hideoutBedTestRoom=-1;
    for (int room=0;room<Hideout::InitialRoomCount;++room) {
        const float bedX=room*6.f+1.1f;
        const float bedDx=p.x-bedX,bedDz=p.z+4.8f;
        if (p.x>=room*6.f && p.x<(room+1)*6.f &&
            bedDx*bedDx+bedDz*bedDz<2.25f) {
            hideoutBedTestRoom=room;
            break;
        }
    }
    hideoutBedTestPrompt=hideoutBedTestRoom>=0;
    if (!hideoutPrompt && hideoutBedTestPrompt && Input::GetKeyDown(GLFW_KEY_F)) {
        BeginHideoutBedPoseTest(hideoutBedTestRoom);
        return;
    }
    if (!hideoutPrompt && nearSwitchNpc && Input::GetKeyDown(GLFW_KEY_F)) {
        hideoutPrompt = 2;
        hideoutPromptAppearance=interactionAppearance;
        hideoutPromptRoom=interactionRoom;
        player->StopMovement();
        Input::SetCursorCaptured(false);
    } else if (!hideoutPrompt && hideoutFurniture>=0 && Input::GetKeyDown(GLFW_KEY_F)) {
        hideoutStoragePanel = true;
        hideoutPanel = false;
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
    if (!hideoutCameraModelHidden)
        playerVisual.Render(*player,shader,camera,float(width),float(height),glm::vec3(1),false);
    hideoutSwitchNpcVisual.Render(hideoutSwitchNpc,shader,camera,float(width),float(height),glm::vec3(1),false);
    for (std::size_t i=0;i<hideoutResidentNpcs.size();++i)
        hideoutResidentVisuals[i]->Render(*hideoutResidentNpcs[i],shader,camera,
            float(width),float(height),glm::vec3(1),false);
    if (hideoutStoragePanel) {
        const bool wasStoragePanelOpen=hideoutStoragePanel;
        hud.RenderHideoutStorage(hideout,hideoutFurnitureRoom,hideoutFurniture,
            *player,playerVisual,shader,hideoutStoragePanel,hideoutExitMenu);
        if (wasStoragePanelOpen&&!hideoutStoragePanel)
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
    if (hideoutBedPoseActive||hideoutBedTestPrompt) {
        ImGui::SetNextWindowPos({width*.5f,height*.76f},ImGuiCond_Always,{.5f,0.f});
        ImGui::Begin("침대 자세 테스트",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted(hideoutBedPoseActive
            ? "[F] 자세 종료 · [Esc] 자세 종료"
            : "[F] 레이드 파밍 자세 테스트");
        if (hideoutBedPoseActive)
            ImGui::TextUnformatted(hideoutBedPoseLooting
                ? "침대 위 적 시체 파밍 중"
                : "침대 위 적 사망 · 파밍 준비 중");
        ImGui::End();
    }
    if (hideout.HasRescuedSurvivor(Hideout::InitialRoomCount)) {
        ImGui::SetNextWindowPos({16,16},ImGuiCond_Always);
        ImGui::Begin("구조 인원 테스트",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("[Y] 3번 방 구조 인원 내보내기");
        ImGui::End();
    }
    if (hideoutPrompt) {
        ImGui::SetNextWindowPos({width*.5f,height*.5f},ImGuiCond_Always,{.5f,.5f});
        ImGui::Begin("은신처 상호작용",nullptr,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        const auto nextAppearance = hideoutPromptAppearance;
        ImGui::TextUnformatted(hideoutPrompt==1 ? "레이드에 진입하시겠습니까?" :
            nextAppearance == PlayerAppearance::Male ? "남캐로 캐릭터 전환 하시겠습니까?" : "여캐로 캐릭터 전환 하시겠습니까?");
        ImGui::BeginDisabled(!Input::IsFocused());
        if(ImGui::Button("예",{120,36})) {
            if(hideoutPrompt==1) deployRequested=true;
            else if(hideout.ChangeAppearance(nextAppearance,hideoutPromptRoom)) {
                player->appearance=hideout.appearance;
                player->clothingMask=hideout.clothingMask;
                playerVisual.Reset();
                playerVisual.Init(hideout.appearance);
                if (hideoutPromptRoom>=Hideout::InitialRoomCount) {
                    HideoutMap::Build(collisionWorld,mapBatches,
                        hideout.AvailableRoomCount(),hideout.rescuedRoomMask);
                }
                ConfigureHideoutSwitchNpc();
            }
            hideoutPrompt=0;
            hideoutPromptRoom=-1;
            Input::SetCursorCaptured(true);
        }
        ImGui::SameLine();
        if(ImGui::Button("아니요",{120,36})) {
            hideoutPrompt=0;
            hideoutPromptRoom=-1;
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
