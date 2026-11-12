#include "../Raid/RaidScene.h"
#include "../Raid/RaidResultScene.h"
#include "../World/IndustrialDetails.h"
#include "../World/HideoutMap.h"
#include "../World/HideoutLayout.h"
#include "../Inventory/InventoryCommands.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "../../Core/Input.h"
#include "../../Scene/SceneManager.h"
#include "../../Graphics/Renderer.h"
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

void RaidScene::EnterHideout()
{
    // Recover legacy zero-water incapacitated profiles without clearing ordinary injuries.
    if (hideout.vitals.Hydration() <= 0.f && !hideout.vitals.Alive()) {
        auto body=hideout.vitals.BodyHealth();
        for (float& health : body) health=(std::max)(health,51.f);
        hideout.vitals.Restore(body,100.f,hideout.vitals.Stamina());
    }
    hideout.vitals.RefillHydration();
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
    if (!player || room<0 || room>=Hideout::HideoutRoomCount ||
        (room>=Hideout::InitialRoomCount && !hideout.HasRescuedSurvivor(room))) return;
    hideoutBedBeforePlayerPosition=player->position;
    hideoutBedBeforePlayerRotation=player->rotation.y;
    hideoutBedBeforeNpcPosition=hideoutSwitchNpc.position;
    hideoutBedBeforeNpcRotation=hideoutSwitchNpc.rotation.y;
    hideoutBedBeforeCamera=camera;
    hideoutBedTestRoom=room;
    // Mattress top: .58 + (.20 * .9) / 2.
    const glm::vec3 bedPosition=HideoutLayout::Anchor(room,"bed")+glm::vec3(0,.67f,0);
    hideoutBedNpcMoved=true;
    if (hideoutBedNpcMoved) {
        hideoutSwitchNpc.position=bedPosition;
        hideoutSwitchNpc.rotation.y=0.f;
        CharacterVisualState npcState;
        npcState.dead=room != 1;
        hideoutSwitchNpcVisual.SetRoomTwoLootPose(room == 1 ? 1 : 0);
        if (room == 1) {
            hideoutSwitchNpc.position.z -= .25f;
            hideoutSwitchNpc.rotation.y = 180.f;
        }
        // Complete the raid death pose on the bed support plane before fitting.
        hideoutSwitchNpcVisual.Update(10.f,hideoutSwitchNpc,npcState);
        if (room != 1) hideoutSwitchNpcVisual.CenterDeathPoseOnSurface(hideoutSwitchNpc,bedPosition);
    }

    playerVisual.SetLootPreview(false);
    playerVisual.SetHideoutKneelPose(false);
    player->SetLooting(false);
    playerVisual.Update(0,*player,{false,false,false,false,0,0,false});
    hideoutBedPoseElapsed=0.f;
    hideoutBedCameraYaw=33.6901f;
    hideoutBedCameraPitch=32.37f;
    hideoutBedPoseLooting=false;
    hideoutBedCorpseAligned=false;
    hideoutCameraInRoom=false;
    hideoutCameraModelHidden=false;
    hideoutCameraBlend=0.f;
    hideoutBedPoseActive=true;
    HideoutMap::Build(collisionWorld,mapBatches,
        hideout.AvailableRoomCount(),hideout.rescuedRoomMask,room);
    BeginHideoutBedLooting();
    hideoutBedTestPrompt=false;
    Input::SetCursorCaptured(true);
}

void RaidScene::BeginHideoutBedLooting()
{
    const glm::vec3 bedPosition=HideoutLayout::Anchor(hideoutBedTestRoom,"bed")+glm::vec3(0,.67f,0);
    if (hideoutBedTestRoom == 1) {
        player->position = bedPosition + glm::vec3(0,0,.3f);
        player->rotation.y = 180.f;
        player->SetLooting(true);
        playerVisual.SetRoomTwoLootPose(2);
        playerVisual.Update(0,*player,{false,false,false,false,0,0,false});
        playerVisual.FitRoomTwoLootHands(*player,hideoutSwitchNpc,hideoutSwitchNpcVisual);
        hideoutBedPoseLooting=true;
        return;
    }
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
    // Keep the raid facing direction and fitted hand targets together.
    // Moving the actor after fitting would move its hands off the corpse.
    player->position=playerVisual.FitLootPose(*player,hideoutSwitchNpc,
        hideoutSwitchNpcVisual,placementClear,180.f,bedPosition.y);
    player->rotation.y=playerVisual.FittedLootYaw();
    player->SetLooting(true);
    playerVisual.Update(0,*player,{false,false,false,false,0,0,false});
    hideoutBedPoseLooting=true;
}

void RaidScene::EndHideoutBedPoseTest()
{
    if (!hideoutBedPoseActive || !player) return;
    player->SetLooting(false);
    playerVisual.SetRoomTwoLootPose(0);
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
    HideoutMap::Build(collisionWorld,mapBatches,
        hideout.AvailableRoomCount(),hideout.rescuedRoomMask);
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
        npcState.dead=hideoutBedTestRoom != 1;
        hideoutSwitchNpcVisual.Update(dt,hideoutSwitchNpc,npcState);
        playerVisual.Update(dt,*player,{false,false,false,false,0,0,false});
        if (hideoutBedTestRoom == 1)
            playerVisual.FitRoomTwoLootHands(*player,hideoutSwitchNpc,hideoutSwitchNpcVisual);
        const glm::vec3 center=HideoutLayout::Anchor(hideoutBedTestRoom,"bed")+glm::vec3(0,.9f,0);
        if (Input::IsFocused() &&
            (Input::GetKey(GLFW_KEY_LEFT_ALT) || Input::GetKey(GLFW_KEY_RIGHT_ALT))) {
            const auto delta=Input::GetMouseDelta();
            hideoutBedCameraYaw-=delta.x*.2f;
            hideoutBedCameraPitch=std::clamp(hideoutBedCameraPitch-delta.y*.2f,-10.f,80.f);
        }
        const float yaw=glm::radians(hideoutBedCameraYaw);
        const float pitch=glm::radians(hideoutBedCameraPitch);
        const float radius=5.9657f;
        camera.position=center+radius*glm::vec3(std::cos(pitch)*std::sin(yaw),
            std::sin(pitch),std::cos(pitch)*std::cos(yaw));
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
        } else if (Input::GetKeyDown(GLFW_KEY_F) || Input::GetKeyDown(GLFW_KEY_TAB)) {
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
    if (Input::IsFocused() && Input::GetKeyDown(GLFW_KEY_TAB)) {
        hideoutPanel = true;
        hideoutStoragePanel = false;
        hud.SetHideoutHealthTab(false);
        player->StopMovement();
        player->SetSprinting(false);
        Input::SetCursorCaptured(false);
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
    player->GetInventory()=hideout.loadout;
    player->clothingMask=hideout.clothingMask;
    hideout.floorLoot.Update(dt,*player,camera,collisionWorld,Input::IsFocused(),false);
    if (!hideoutPrompt && hideout.floorLoot.Target() && Input::GetKeyDown(GLFW_KEY_F)) {
        const auto id=hideout.floorLoot.Target()->stack.id;
        hideout.TakeFromFloor({InventoryCommands::Action::Move,{3,id},0},*player,collisionWorld);
        return;
    }
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
    if (p.x>.2f && p.x<roomRight && p.z < .25f && p.z > -8.f) {
        hideoutFurnitureRoom = (std::min)(Hideout::HideoutRoomCount-1,
            static_cast<int>(p.x/6.f));
        hideoutFurniture=HideoutLayout::NearestStorage(hideoutFurnitureRoom,p);
    }
    hideoutBedTestRoom=-1;
    for (int room=0;room<Hideout::HideoutRoomCount;++room) {
        if (room>=Hideout::InitialRoomCount && !hideout.HasRescuedSurvivor(room)) continue;
        const auto bed=HideoutLayout::Anchor(room,"bed");
        const float bedDx=p.x-bed.x,bedDz=p.z-bed.z;
        const float radius=HideoutLayout::InteractionRadius("bed");
        if (p.x>=room*6.f && p.x<(room+1)*6.f &&
            bedDx*bedDx+bedDz*bedDz<radius*radius) {
            hideoutBedTestRoom=room;
            break;
        }
    }
    hideoutBedTestPrompt=hideoutBedTestRoom>=0;
    if (!hideoutPrompt && hideoutBedTestPrompt && Input::GetKeyDown(GLFW_KEY_F)) {
        BeginHideoutBedPoseTest(hideoutBedTestRoom);
        return;
    }
    if (!hideoutPrompt && nearSwitchNpc && hideoutFurniture<0 && Input::GetKeyDown(GLFW_KEY_F)) {
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
    hideout.floorLoot.Render(shader,cubeMesh,camera,float(width),float(height));
    if (!hideoutCameraModelHidden)
        playerVisual.Render(*player,shader,camera,float(width),float(height),glm::vec3(1),false);
    hideoutSwitchNpcVisual.Render(hideoutSwitchNpc,shader,camera,float(width),float(height),glm::vec3(1),false);
    for (std::size_t i=0;i<hideoutResidentNpcs.size();++i)
        hideoutResidentVisuals[i]->Render(*hideoutResidentNpcs[i],shader,camera,
            float(width),float(height),glm::vec3(1),false);
    if (hideoutStoragePanel) {
        const bool wasStoragePanelOpen=hideoutStoragePanel;
        hud.RenderHideoutStorage(hideout,hideoutFurnitureRoom,hideoutFurniture,
            *player,playerVisual,shader,hideoutStoragePanel,hideoutExitMenu,collisionWorld);
        if (wasStoragePanelOpen&&!hideoutStoragePanel)
            Input::SetCursorCaptured(true);
        return;
    }
    if (hideoutPanel) {
        const bool wasExitMenuOpen=hideoutExitMenu;
        hud.RenderHideout(hideout,*player,playerVisual,shader,true,&hideoutExitMenu,-1,-1,&collisionWorld);
        if (wasExitMenuOpen && !hideoutExitMenu)
            Input::SetCursorCaptured(false);
        return;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (!hideoutPrompt && !hideoutBedPoseActive && !hideoutExitMenu) {
        ImGui::SetNextWindowPos({width*.5f,height*.86f},ImGuiCond_Always,{.5f,0.f});
        ImGui::Begin("은신처 보관함 안내",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_AlwaysAutoResize);
        if (const auto* item=hideout.floorLoot.Target())
            ImGui::Text("[F] %s 회수",GetItemDefinition(item->stack.type).name);
        else if (hideoutFurniture==Hideout::PelicanStorage)
            ImGui::TextUnformatted("[F] 펠리컨 케이스 열기 · 창고");
        else if (hideoutFurniture>=0)
            ImGui::TextUnformatted("[F] 보관함 열기");
        ImGui::End();
    }
    if (hideoutBedPoseActive||hideoutBedTestPrompt) {
        ImGui::SetNextWindowPos({width*.5f,height*.76f},ImGuiCond_Always,{.5f,0.f});
        ImGui::Begin("침대 자세 테스트",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted(hideoutBedPoseActive
            ? "[F/Esc] 자세 종료 · [Alt + 마우스] 카메라 회전"
            : (hideoutBedTestRoom == 1 ? "[F] 새 파밍 자세 테스트" : "[F] 레이드 파밍 자세 테스트"));
        if (hideoutBedPoseActive)
            ImGui::TextUnformatted(hideoutBedPoseLooting
                ? (hideoutBedTestRoom == 1 ? "적: 손·무릎 지지 / 플레이어: 뒤에서 장비 수색" : "침대 위 적 시체 파밍 중")
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
        if (hideoutPrompt == 1) {
            ImGui::TextUnformatted(hideout.appearance == PlayerAppearance::Female
                ? "SCAV: 착용 장비와 휴대품을 창고에 자동 보관하고 무작위 장비로 출격합니다."
                : "PMC: 현재 착용 장비와 휴대품을 가지고 출격합니다.");
            if (hideout.appearance == PlayerAppearance::Female)
                ImGui::TextUnformatted("창고 공간이 부족하면 출격할 수 없습니다.");
        }
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
