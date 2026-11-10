#include "../Inventory/RaidInventoryCommands.h"
#include "../../Core/AssetCatalog.h"
#include <Windows.h>
#include "RaidHud.h"
#include "../Raid/RaidStatus.h"
#include "../Raid/RaidSummary.h"
#include "../Hideout/Hideout.h"
#include "../World/IndustrialZone.h"
#include <cmath>
#include <algorithm>
#include "../Inventory/Inventory.h"
#include "InventoryUi.h"
#include "../Character/Player.h"
#include "../Combat/CombatSystem.h"
#include "../Inventory/InventoryActions.h"
#include "../../Core/Input.h"
#include "../Combat/LootSystem.h"
#include "../../Animation/CharacterVisual.h"
#include "../../Graphics/Camera.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Shader.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>
#include <filesystem>
#include <array>
#include <vector>

#include "HudWidgets.h"
using namespace HudWidgets;

RaidHud::RaidHud() = default;

RaidHud::~RaidHud() { Shutdown(); }

void RaidHud::Init(GLFWwindow* window)
{
    Shutdown();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    contextReady = true;
    ImGui::GetIO().IniFilename = nullptr;
    // Use the system Korean font; ImGui's built-in font has no Hangul glyphs.
    wchar_t windowsDirectory[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        throw std::runtime_error("Cannot locate Windows fonts");
    auto& io = ImGui::GetIO();
    const auto fonts = std::filesystem::path(windowsDirectory) / "Fonts";
    for (const auto* filename : { L"malgun.ttf", L"gulim.ttc" }) {
        const auto path = fonts / filename;
        if (std::filesystem::exists(path))
            io.FontDefault = io.Fonts->AddFontFromFileTTF(path.u8string().c_str(), 17.0f,
                nullptr, io.Fonts->GetGlyphRangesKorean());
        if (io.FontDefault) break;
    }
    if (!io.FontDefault)
        throw std::runtime_error("Korean UI requires Malgun Gothic or Gulim in Windows Fonts");
    if (!itemImages.Load(AssetCatalog::UI_items_png)) throw std::runtime_error("Missing inventory image atlas");
    ImGui::StyleColorsDark();
    windowReady = ImGui_ImplGlfw_InitForOpenGL(window, true);
    if (!windowReady) throw std::runtime_error("HUD window initialization failed");
    rendererReady = ImGui_ImplOpenGL3_Init("#version 330");
    if (!rendererReady) throw std::runtime_error("HUD renderer initialization failed");
}

void RaidHud::Render(Player& player, LootSystem& loot, CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid,
    const CollisionWorld& world, Shader& shader, Enemy* corpse, int corpseTarget, StackId* takeStack,
    bool* takeWeapon, bool* closeLoot, bool* rescuePromptOpen, bool* rescueRequested)
{
    if (!rendererReady) return;
    auto& inventory = player.GetInventory();
    if (!inventoryPreviewVisual) {
        inventoryPreviewVisual=std::make_unique<CharacterVisual>();
        inventoryPreviewVisual->Init(player.appearance);
    }
    Player inventoryCharacter;
    inventoryCharacter.clothingMask=player.clothingMask;
    inventoryCharacter.SetLooting(corpse!=nullptr);
    CharacterPreview inventoryPreview{inventoryPreviewVisual.get(),&inventoryCharacter,&shader,{}, {}, {}, ImGui::GetIO().DisplayFramebufferScale,combat.HasWeapon()};
    inventoryPreview.camera.position={0,1.f,3.4f};
    inventoryPreview.camera.front={0,0,-1}; inventoryPreview.camera.up={0,1,0}; inventoryPreview.camera.fieldOfView=38;
    if (corpse) {
        inventoryPreview.camera.position={0,.65f,3.1f};
    }
    // Render the enemy's existing skeletal/death pose without advancing its animation twice.
    Character enemyCharacter = corpse ? corpse->body : Character{};
    enemyCharacter.position = {0,0,0};
    enemyCharacter.rotation.y = enemyPreviewRotation;
    CharacterPreview enemyPreview{corpse ? corpse->visual.get() : nullptr,&enemyCharacter,&shader,
        inventoryPreview.camera,{}, {},ImGui::GetIO().DisplayFramebufferScale,corpse && corpse->rifle,false};
    enemyPreview.camera.position = {0,2.2f,3.5f};
    enemyPreview.camera.front = glm::normalize(glm::vec3(0,.25f,0)-enemyPreview.camera.position);
    enemyPreview.camera.fieldOfView = 48;
    inventoryCharacter.rotation.y=inventoryPreviewRotation;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    const ImGuiWindowFlags overlay = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16, 16), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::Begin("레이드", nullptr, raid.Finished() ? (overlay & ~ImGuiWindowFlags_NoInputs) : overlay);
    const int seconds = static_cast<int>(std::ceil(raid.remaining));
    ImGui::Text("%02d:%02d", seconds / 60, seconds % 60);
    ImGui::Text("위치 X %.0f / Z %.0f", player.position.x, player.position.z);
    if (raid.phase == RaidPhase::Active) {
        const auto& exit = IndustrialZone::Exits()[raid.assignedExit];
        ImGui::Text("지정 탈출구: %s | %.0fm", exit.name, raid.exitDistance);
        ImGui::Text("탈출구 X %.0f / Z %.0f", exit.position.x, exit.position.z);
        if (raid.extraction > 0)
            ImGui::Text("탈출까지 %.1f초", RaidConfig::DefaultExtractTime - raid.extraction);
    }
    else {
        ImGui::TextUnformatted(raid.phase == RaidPhase::Extracted ? "생존 및 탈출 성공" :
            raid.phase == RaidPhase::Dead ? "전사 - 장비 손실" : "실종 - MIA 처리, 장비 손실");
        ImGui::Text("회수한 물품: %d묶음 | %.2f kg", static_cast<int>(raid.recovered.Items().size()), raid.recovered.TotalWeight());
        for (const auto& stack : raid.recovered.Items())
            ImGui::Text("%s x%d", GetItemDefinition(stack.type).name, stack.quantity);
        for (int slot = 0; slot < ClothingSlotCount; ++slot) {
            const auto type = ClothingType(slot,player.appearance == PlayerAppearance::Male);
            if (raid.recoveredClothing & ClothingBit(type)) ImGui::Text("착용 회수: %s", GetItemDefinition(type).name);
        }
        ImGui::TextUnformatted(raid.saveMessage.c_str());
        ImGui::BeginDisabled(!Input::IsFocused());
        if (ImGui::Button("은신처로 돌아가기")) raid.returnToHideout = true;
        ImGui::EndDisabled();
    }
    ImGui::End();

    if (raid.phase == RaidPhase::Active) {
        auto* draw = ImGui::GetForegroundDrawList();
        const float scale = 0.5f * (std::min)(0.78f, (std::max)(0.45f, io.DisplaySize.y / 900.0f));
        const ImU32 silhouette=IM_COL32(220,226,216,235);
        const float bottom=io.DisplaySize.y-24;
        DrawStanceSilhouette(draw,{18,bottom-150},player.IsCrouching(),player.IsProne(),silhouette);
        draw->AddText({93,bottom-71},silhouette,player.IsProne()?"엎드림":player.IsCrouching()?"앉음":"섬");
        auto gauge=[&](float y,float value,ImU32 color,const char* label) {
            draw->AddText({18,y-17},silhouette,label);
            draw->AddRectFilled({18,y},{178,y+7},IM_COL32(30,35,32,220),2);
            draw->AddRectFilled({18,y},{18+160*std::clamp(value/100.f,0.f,1.f),y+7},color,2);
        };
        gauge(bottom-30,player.vitals.Stamina(),IM_COL32(164,190,120,255),"스태미나");
        gauge(bottom,player.vitals.Hydration(),IM_COL32(91,165,211,255),"수분");
        if (combat.ShowFireMode()) draw->AddText({io.DisplaySize.x-175,bottom},silhouette,combat.FireModeName());
        if (!combat.Message().empty()) draw->AddText({io.DisplaySize.x*.4f,bottom-30},silhouette,combat.Message().c_str());
        float nextY = 16.0f;
        const float hudDelta = std::clamp(io.DeltaTime, 0.0f, 0.1f);
        ammoDisplaySeconds = (std::max)(0.0f, ammoDisplaySeconds - hudDelta);
        const bool reloading = combat.ReloadTime() > 0.0f;
        if (reloading && !reloadingPreviousFrame) ammoDisplaySeconds = 3.0f;
        reloadingPreviousFrame = reloading;
        if (!inventoryOpen && Input::IsFocused() && Input::GetKey(GLFW_KEY_R)) {
            reloadHoldSeconds += hudDelta;
            if (reloadHoldSeconds >= 1.0f && !reloadHoldTriggered) {
                ammoDisplaySeconds = 3.0f;
                reloadHoldTriggered = true;
            }
        } else {
            reloadHoldSeconds = 0.0f;
            reloadHoldTriggered = false;
        }
        if (actions.healingMode) {
            const ImVec2 origin(16, nextY);
            DrawBodyMannequin(player.vitals, origin, scale, draw);
            {
                ImGui::SetNextWindowPos(origin);
                ImGui::SetNextWindowSize({145 * scale + 20, 275 * scale + 20});
                ImGui::Begin("치료 부위 선택", nullptr, ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoBringToFrontOnFocus);
                BodyPartButtons(player.vitals, origin, scale, "RaidHeal", Input::IsFocused(),
                    [&](BodyPart part) { actions.RequestHeal(part); });
                draw->AddText({16, origin.y + 264 * scale}, IM_COL32(238, 222, 170, 240),
                    "치료 부위 클릭");
                ImGui::End();
            }
            nextY += 310 * scale;
        }
        if (ammoDisplaySeconds > 0.0f) {
            draw->AddRectFilled({16, nextY}, {235, nextY + 30}, IM_COL32(20, 25, 28, 170), 4);
            const std::string ammunition = "잔탄 " + std::to_string(combat.Magazine()) +
                " / " + std::to_string(combat.ActiveWeapon()?MagazineCapacity(combat.ActiveWeapon()->type):30) + "   낱탄 " + std::to_string(inventory.Count(ItemType::Ammo));
            draw->AddText({23, nextY + 7}, IM_COL32(232, 227, 204, 235), ammunition.c_str());
        }
    }

    if (raid.phase == RaidPhase::Active && !inventoryOpen && !corpse)
    {
        const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        if (combat.Aiming() && player.vitals.Alive() && Input::IsFocused()) {
            auto* draw = ImGui::GetForegroundDrawList();
            const ImU32 color = loot.Target() ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 255, 255, 220);
            draw->AddLine(ImVec2(center.x - 5, center.y), ImVec2(center.x + 5, center.y), color);
            draw->AddLine(ImVec2(center.x, center.y - 5), ImVec2(center.x, center.y + 5), color);
        }
        if (corpseTarget >= 0 && player.vitals.Alive() && Input::IsFocused()) {
            ImGui::SetNextWindowPos(ImVec2(center.x, center.y + 28), ImGuiCond_Always, ImVec2(.5f, 0));
            ImGui::Begin("시체 상호작용", nullptr, overlay);
            ImGui::TextUnformatted("[F] 파밍");
            ImGui::End();
        }
        // Target() already requires a center-screen ray hit, reach and clear line of sight.
        if (const auto* item = loot.Target(); item && player.vitals.Alive() && Input::IsFocused()) {
            ImGui::SetNextWindowPos(ImVec2(center.x, center.y + 28), ImGuiCond_Always, ImVec2(0.5f, 0));
            ImGui::Begin("상호작용", nullptr, overlay);
            ImGui::Text("[F] %s x%d", GetItemDefinition(item->stack.type).name, item->stack.quantity);
            ImGui::End();
        }
        if (!loot.Message().empty()) {
            ImGui::SetNextWindowPos(ImVec2(center.x, io.DisplaySize.y - 32), ImGuiCond_Always, ImVec2(0.5f, 1));
            ImGui::Begin("획득 결과", nullptr, overlay);
            ImGui::TextUnformatted(loot.Message().c_str());
            ImGui::End();
        }
    }
    if (raid.phase == RaidPhase::Active && (inventoryOpen || corpse))
        RenderInventoryPanel(player,loot,actions,world,corpse,inventoryPreview,enemyPreview,takeWeapon,closeLoot);
    if (rescuePromptOpen && *rescuePromptOpen) {
        ImGui::SetNextWindowPos({io.DisplaySize.x*.5f,io.DisplaySize.y*.5f},
            ImGuiCond_Always,{.5f,.5f});
        ImGui::Begin("생존자 구조",nullptr,
            ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("이 여성을 은신처로 데려가시겠습니까?");
        if (ImGui::Button("데려간다",{140,40})) {
            if (rescueRequested) *rescueRequested=true;
            *rescuePromptOpen=false;
        }
        ImGui::SameLine();
        if (ImGui::Button("두고간다",{140,40})) {
            *rescuePromptOpen=false;
            Input::SetCursorCaptured(true);
        }
        ImGui::End();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RaidHud::Shutdown()
{
    inventoryPreviewVisual.reset();
    itemImages.Reset();
    if (rendererReady) ImGui_ImplOpenGL3_Shutdown();
    if (windowReady) ImGui_ImplGlfw_Shutdown();
    if (contextReady) ImGui::DestroyContext();
    rendererReady = windowReady = contextReady = false;
}

void RaidHud::ResetAmmoIndicator()
{
    reloadHoldSeconds = 0.f;
    ammoDisplaySeconds = 0.f;
    reloadHoldTriggered = false;
    reloadingPreviousFrame = false;
}
