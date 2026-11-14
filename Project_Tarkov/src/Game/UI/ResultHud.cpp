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

bool RaidHud::RenderExtractionSummary(const RaidSummary& summary, Player& character,
    CharacterVisual& visual, Shader& shader, Character* rescuedCharacter,
    CharacterVisual* rescuedVisual)
{
    if (!rendererReady) return false;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const auto& io = ImGui::GetIO();
    const ImVec4 accent(.78f, .68f, .42f, 1);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("탈출 결과", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    const bool extracted = summary.outcome == RaidSummary::Outcome::Extracted;
    ImGui::TextColored(extracted ? ImVec4(.40f,.90f,.55f,1) : ImVec4(.95f,.30f,.30f,1),
        extracted ? "탈출 성공" : "탈출실패");
    ImGui::SameLine();
    ImGui::TextDisabled("/ 산업지대 레이드 결과");
    ImGui::Separator();

    ImGui::BeginChild("결과 캐릭터", {0, io.DisplaySize.y * 0.43f}, true);
    std::vector<CharacterPreview> previews;
    previews.reserve(2);
    auto drawPreview=[&](const char* childId,const char* label,Character& model,
        CharacterVisual& modelVisual,bool hasRifle,float childWidth=0.f) {
        ImGui::BeginChild(childId,{childWidth,0},false);
        ImGui::TextUnformatted(label);
        const ImVec2 modelPosition=ImGui::GetCursorScreenPos();
        const ImVec2 modelSize(ImGui::GetContentRegionAvail().x,
            (std::max)(180.f,ImGui::GetContentRegionAvail().y-4.f));
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(modelPosition,{modelPosition.x+modelSize.x,modelPosition.y+modelSize.y},
            IM_COL32(25,29,26,235),3.f);
        previews.push_back({&modelVisual,&model,&shader,{},modelPosition,modelSize,
            io.DisplayFramebufferScale,hasRifle});
        auto& preview=previews.back();
        preview.camera.position={0,1.f,3.4f};
        preview.camera.front={0,0,-1};
        preview.camera.up={0,1,0};
        preview.camera.fieldOfView=38.f;
        draw->AddCallback(DrawCharacterPreview,&preview);
        draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        ImGui::Dummy(modelSize);
        ImGui::EndChild();
    };
    if (summary.rescuedFemale && rescuedCharacter && rescuedVisual) {
        const float modelColumn=ImGui::GetContentRegionAvail().x*.5f-4.f;
        drawPreview("플레이어 모델","플레이어",character,visual,summary.recoveredRifle,modelColumn);
        ImGui::SameLine();
        drawPreview("합류한 여캐 모델","여캐 (합류함)",*rescuedCharacter,
            *rescuedVisual,false,modelColumn);
    } else {
        drawPreview("플레이어 모델","플레이어",character,visual,summary.recoveredRifle);
    }
    ImGui::EndChild();

    ImGui::TextColored(accent, "소요 시간");
    ImGui::SameLine();
    ImGui::Text("%02d분 %02d초", summary.durationSeconds / 60, summary.durationSeconds % 60);

    const float detailsHeight = (std::max)(120.0f, io.DisplaySize.y - 590.0f);
    const float columnWidth = (io.DisplaySize.x - 60.0f) * 0.5f;
    ImGui::BeginChild("처치 기록", {columnWidth, detailsHeight}, true);
    ImGui::TextColored(accent, "처치 기록");
    if (summary.kills.empty()) {
        ImGui::TextUnformatted("처치한 적이 없습니다.");
    } else {
        for (const auto& kill : summary.kills)
            ImGui::Text("%s | %s | %s | 피격 부위: %s",
                kill.time.c_str(), kill.target.c_str(), kill.location.c_str(), kill.hitLocation.c_str());
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild(extracted ? "획득 장비" : "손실 안내", {0, detailsHeight}, true);
    ImGui::TextColored(accent, extracted ? "획득 장비 (기존 장비 제외)" : "레이드 결과");
    if (!extracted) {
        ImGui::TextWrapped("출격 장비와 레이드에서 획득한 물품을 잃었습니다. 은신처에 보관한 물품은 유지됩니다.");
        if (!summary.lostEquipment.empty()) {
            ImGui::Spacing();
            ImGui::TextUnformatted("손실 장비");
            for (const auto& equipment : summary.lostEquipment)
                ImGui::BulletText("%s", equipment.c_str());
        }
    } else
    if (summary.acquiredEquipment.empty()) {
        ImGui::TextUnformatted("새로 획득한 장비가 없습니다.");
    } else {
        for (const auto& equipment : summary.acquiredEquipment)
            ImGui::BulletText("%s", equipment.c_str());
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::SetCursorPosX((std::max)(0.0f, (io.DisplaySize.x - 250.0f) * 0.5f));
    const bool returnToHideout = ImGui::Button("은신처로 돌아가기", {250, 48});
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return returnToHideout;
}
