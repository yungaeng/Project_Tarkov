#include <Windows.h>
#include "RaidHud.h"
#include "RaidStatus.h"
#include "../RaidSummary.h"
#include "Hideout.h"
#include "IndustrialZone.h"
#include <cmath>
#include <algorithm>
#include "Inventory.h"
#include "Player.h"
#include "CombatSystem.h"
#include "InventoryActions.h"
#include "../Core/Input.h"
#include "LootSystem.h"
#include "../Animation/CharacterVisual.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Shader.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>
#include <filesystem>
#include <array>

namespace
{
    std::array<ImVec4, BodyPartCount> BodyPartRects(ImVec2 origin, float scale)
    {
        const float rectangles[BodyPartCount][4]{
            {52, 0, 84, 35}, {39, 43, 97, 112}, {42, 120, 94, 153},
            {8, 47, 31, 133}, {105, 47, 128, 133}, {43, 163, 68, 254}, {77, 163, 102, 254}
        };
        std::array<ImVec4, BodyPartCount> result{};
        for (int i = 0; i < BodyPartCount; ++i)
            result[i] = {origin.x + rectangles[i][0] * scale, origin.y + rectangles[i][1] * scale,
                origin.x + rectangles[i][2] * scale, origin.y + rectangles[i][3] * scale};
        return result;
    }

    ImU32 BodyPartColor(float health)
    {
        if (health <= 10) return IM_COL32(10, 12, 12, 180);
        if (health <= 50) return IM_COL32(210, 48, 42, 155);
        return IM_COL32(220, 226, 220, 92);
    }

    void DrawBodyMannequin(const PlayerVitals& vitals, ImVec2 origin, float scale, ImDrawList* target = nullptr)
    {
        auto* draw = target ? target : ImGui::GetWindowDrawList();
        const auto rectangles = BodyPartRects(origin, scale);
        const float headRadius = 16 * scale;
        draw->AddCircleFilled({origin.x + 68 * scale, origin.y + 17 * scale}, headRadius,
            BodyPartColor(vitals.BodyHealth(BodyPart::Head)), 20);
        for (int i = 1; i < BodyPartCount; ++i) {
            const auto& rectangle = rectangles[static_cast<std::size_t>(i)];
            draw->AddRectFilled({rectangle.x, rectangle.y}, {rectangle.z, rectangle.w},
                BodyPartColor(vitals.BodyHealth(static_cast<BodyPart>(i))), 5 * scale);
        }
        for (int i = 0; i < BodyPartCount; ++i) {
            const auto& rectangle = rectangles[static_cast<std::size_t>(i)];
            if (i == static_cast<int>(BodyPart::Head))
                draw->AddCircle({origin.x + 68 * scale, origin.y + 17 * scale}, 16 * scale,
                    IM_COL32(238, 235, 218, 100), 20, 1 * scale);
            else draw->AddRect({rectangle.x, rectangle.y}, {rectangle.z, rectangle.w},
                IM_COL32(238, 235, 218, 100), 5 * scale, 0, 1 * scale);
        }
    }

    template<typename Handler>
    void BodyPartButtons(const PlayerVitals& vitals, ImVec2 origin, float scale, const char* id,
        bool enabled, Handler&& handler)
    {
        const auto rectangles = BodyPartRects(origin, scale);
        for (int i = 0; i < BodyPartCount; ++i) {
            const auto& rectangle = rectangles[static_cast<std::size_t>(i)];
            ImGui::SetCursorScreenPos({rectangle.x, rectangle.y});
            ImGui::PushID(id);
            ImGui::InvisibleButton(BodyPartNames[i], {rectangle.z - rectangle.x, rectangle.w - rectangle.y});
            if (enabled && ImGui::IsItemClicked(ImGuiMouseButton_Left))
                handler(static_cast<BodyPart>(i));
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s: %.0f / 100",BodyPartNames[i],vitals.BodyHealth(static_cast<BodyPart>(i)));
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
    }
}

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
    ImGui::StyleColorsDark();
    windowReady = ImGui_ImplGlfw_InitForOpenGL(window, true);
    if (!windowReady) throw std::runtime_error("HUD window initialization failed");
    rendererReady = ImGui_ImplOpenGL3_Init("#version 330");
    if (!rendererReady) throw std::runtime_error("HUD renderer initialization failed");
}

void RaidHud::Render(const Player& player, const LootSystem& loot, const CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid,
    const Enemy* corpse, int corpseTarget, StackId* takeStack, bool* takeWeapon, bool* closeLoot)
{
    if (!rendererReady) return;
    const auto& inventory = player.GetInventory();
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
        const auto& exit = IndustrialZone::Exits[raid.assignedExit];
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
            const auto type = ClothingType(slot);
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
        if (player.vitals.Health() < 100 || actions.healingMode) {
            const ImVec2 origin(16, nextY);
            DrawBodyMannequin(player.vitals, origin, scale, draw);
            if (actions.healingMode) {
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
        if (player.vitals.Hydration() <= 10) {
            const ImVec2 low(16, nextY);
            const ImVec2 high(74, nextY + 54);
            draw->AddRectFilled(low, high, IM_COL32(20, 25, 28, 100), 5);
            draw->AddCircleFilled({45, nextY + 34}, 11, IM_COL32(96, 190, 221, 135), 20);
            draw->AddTriangleFilled({45, nextY + 6}, {34, nextY + 33}, {56, nextY + 33},
                IM_COL32(96, 190, 221, 135));
            nextY += 62;
        }
        if (player.vitals.Stamina() <= 10) {
            const ImVec2 low(16, nextY);
            const ImVec2 high(74, nextY + 54);
            draw->AddRectFilled(low, high, IM_COL32(20, 25, 28, 100), 5);
            const ImU32 runner = IM_COL32(220, 226, 220, 190);
            draw->AddCircleFilled({46, nextY + 12}, 5, runner, 16);
            draw->AddLine({44, nextY + 19}, {39, nextY + 30}, runner, 4);
            draw->AddLine({42, nextY + 21}, {31, nextY + 25}, runner, 3);
            draw->AddLine({42, nextY + 21}, {53, nextY + 18}, runner, 3);
            draw->AddLine({39, nextY + 30}, {28, nextY + 40}, runner, 4);
            draw->AddLine({28, nextY + 40}, {19, nextY + 38}, runner, 4);
            draw->AddLine({39, nextY + 30}, {49, nextY + 37}, runner, 4);
            draw->AddLine({49, nextY + 37}, {59, nextY + 34}, runner, 4);
            nextY += 62;
        }
        if (ammoDisplaySeconds > 0.0f) {
            draw->AddRectFilled({16, nextY}, {235, nextY + 30}, IM_COL32(20, 25, 28, 170), 4);
            const std::string ammunition = "잔탄 " + std::to_string(combat.Magazine()) +
                " / 30   예비 " + std::to_string(inventory.Count(ItemType::Ammo));
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
            ImGui::TextUnformatted("[F] 시체 수색 · 몸을 낮추고 파밍");
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
    else if (raid.phase == RaidPhase::Active && inventoryOpen && !corpse)
    {
        const float inventoryWidth=actions.healingMode?
            (std::max)(160.f,(std::min)(560.f,io.DisplaySize.x-190.f)):
            (io.DisplaySize.x < 560 ? io.DisplaySize.x : 560);
        const float inventoryLeft=actions.healingMode?
            180.f+(std::max)(0.f,io.DisplaySize.x-180.f-inventoryWidth)*.5f:
            (io.DisplaySize.x-inventoryWidth)*.5f;
        const float inventoryCenterX=inventoryLeft+inventoryWidth*.5f;
        ImGui::SetNextWindowPos(ImVec2(inventoryCenterX, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(inventoryWidth,
            io.DisplaySize.y < 420 ? io.DisplaySize.y : 420));
        ImGui::Begin("인벤토리 [I키로 닫기]", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("사용한 칸 %d/%d   가방 무게 %.2f kg", static_cast<int>(inventory.Items().size()),
            inventory.SlotCapacity(), inventory.TotalWeight());
        ImGui::Separator();
        ImGui::TextUnformatted("착용 의상 (가방 공간과 별도)");
        for (int slot = 0; slot < ClothingSlotCount; ++slot) {
            const auto type = ClothingType(slot);
            const bool worn = (player.clothingMask & ClothingBit(type)) != 0;
            ImGui::PushID(400 + slot);
            ImGui::Text("%s: %s", GetItemDefinition(type).name, worn ? "착용 중" : "미착용");
            ImGui::SameLine();
            ImGui::BeginDisabled(!worn || !Input::IsFocused() || !player.vitals.Alive());
            if (ImGui::SmallButton("해제 → 가방")) actions.RequestUnequip(type);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::Separator();
        if (inventory.Items().empty()) ImGui::TextUnformatted("비어 있습니다. 보급품을 찾아 F키로 주우세요.");
        if (ImGui::BeginTable("Items", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("아이템");
            ImGui::TableSetupColumn("수량");
            ImGui::TableSetupColumn("무게 (kg)");
            ImGui::TableHeadersRow();
            for (const auto& stack : inventory.Items())
            {
                const auto& item = GetItemDefinition(stack.type);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto label = std::string(item.name) + "##" + std::to_string(stack.id);
                if (ImGui::Selectable(label.c_str(), actions.selected == stack.id, ImGuiSelectableFlags_SpanAllColumns)) actions.selected = stack.id;
                ImGui::TableNextColumn(); ImGui::Text("%d / %d", stack.quantity, item.maxStack);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", stack.quantity * item.weight);
            }
            ImGui::EndTable();
        }
        if (const auto* stack = inventory.Find(actions.selected))
        {
            const auto& definition = GetItemDefinition(stack->type);
            ImGui::Separator();
            ImGui::Text("%s | %d/%d | 개당 %.2f kg | 묶음 %.2f kg", definition.name, stack->quantity, definition.maxStack, definition.weight, definition.weight * stack->quantity);
            ImGui::TextUnformatted(IsClothing(stack->type) ? "착용하면 캐릭터 외형에 반영됩니다. 해제 후 버릴 수 있습니다." :
                stack->type == ItemType::Bandage ? "효과: 선택한 부위 체력 +20" : stack->type == ItemType::Water ? "효과: 수분 +30" : "탄약: 인벤토리를 닫고 R키로 재장전하세요");
            ImGui::BeginDisabled(!Input::IsFocused() || !player.vitals.Alive());
            const bool usable = IsClothing(stack->type) ? !(player.clothingMask & ClothingBit(stack->type)) :
                stack->type == ItemType::Bandage ? player.vitals.Health() < 100 : stack->type == ItemType::Water && player.vitals.Hydration() < 100;
            ImGui::BeginDisabled(!usable);
            if (ImGui::Button(IsClothing(stack->type) ? "착용" : "사용")) actions.Request(InventoryAction::Use);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("1개 버리기")) actions.Request(InventoryAction::DropOne);
            ImGui::SameLine();
            if (ImGui::Button("모두 버리기")) actions.Request(InventoryAction::DropAll);
            ImGui::EndDisabled();
            if (!usable) ImGui::TextUnformatted(IsClothing(stack->type) ? "이미 같은 부위의 의상을 착용하고 있습니다." : "사용 불가: 회복할 필요가 없거나 탄약 아이템입니다.");
        }
        if (!loot.Message().empty()) ImGui::TextUnformatted(loot.Message().c_str());
        ImGui::End();
    }
    if (corpse && raid.phase == RaidPhase::Active) {
        const float width = LootPanelWidth(io.DisplaySize.x);
        const float height = (std::min)(420.f, (std::max)(220.f, io.DisplaySize.y - 64.f));
        // Open on the right; allow dragging for the duration of this search.
        ImGui::SetNextWindowPos({io.DisplaySize.x - 12.f, io.DisplaySize.y*.5f}, ImGuiCond_Appearing, {1.f,.5f});
        ImGui::SetNextWindowSize({width,height}, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.08f,.10f,.08f,.95f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.22f,.27f,.19f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.35f,.40f,.27f,1));
        ImGui::Begin("시체 수색", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextWrapped("시체를 수색하는 중 · 수색 중에도 공격받을 수 있습니다.");
        ImGui::TextWrapped("제목을 드래그해 창 이동 · 아이템 클릭/드래그로 획득 · F / I / ESC로 닫기");
        ImGui::BeginDisabled(!Input::IsFocused() || !player.vitals.Alive());
        if (ImGui::Button("수색 종료") && closeLoot) *closeLoot = true;
        ImGui::Separator();
        const float paneWidth = (ImGui::GetContentRegionAvail().x - 12.f)*.5f;
        const float paneHeight = (std::max)(80.f, ImGui::GetContentRegionAvail().y - 32.f);
        ImGui::BeginChild("내 휴대품", {paneWidth,paneHeight}, true);
        ImGui::Text("내 휴대품 %d/%d", static_cast<int>(inventory.Items().size()), inventory.SlotCapacity());
        ImGui::Text("무게 %.2f kg", inventory.TotalWeight());
        ImGui::Separator();
        if (ImGui::BeginTable("휴대품 슬롯", 2, ImGuiTableFlags_SizingStretchSame)) {
            for (int i=0; i<inventory.SlotCapacity(); ++i) {
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                std::string label = "빈 칸";
                if (i < static_cast<int>(inventory.Items().size())) {
                    const auto& stack = inventory.Items()[static_cast<std::size_t>(i)];
                    label = std::string(GetItemDefinition(stack.type).name) + "\nx" + std::to_string(stack.quantity);
                }
                ImGui::Button(label.c_str(), {ImGui::GetContentRegionAvail().x,48});
                if (ImGui::BeginDragDropTarget()) {
                    if (const auto* payload=ImGui::AcceptDragDropPayload("CORPSE_ITEM"))
                        if (takeStack && payload->DataSize==sizeof(StackId)) *takeStack=*static_cast<const StackId*>(payload->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("적의 장비와 휴대품", {0,paneHeight}, true);
        ImGui::TextUnformatted("적의 장비 / 휴대품");
        ImGui::Separator();
        if (corpse->rifle) {
            ImGui::BeginDisabled(combat.HasRifle());
            if (ImGui::Button("소총 획득 · 탄창 30발", {-1,40}) && takeWeapon) *takeWeapon=true;
            ImGui::EndDisabled();
            if (combat.HasRifle()) ImGui::TextUnformatted("소총은 한 자루만 휴대할 수 있습니다.");
        }
        if (ImGui::BeginTable("시체 슬롯", 2, ImGuiTableFlags_SizingStretchSame)) {
            for (const auto& stack : corpse->inventory.Items()) {
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(stack.id));
                const auto label=std::string(GetItemDefinition(stack.type).name)+"\nx"+std::to_string(stack.quantity);
                if (ImGui::Button(label.c_str(), {ImGui::GetContentRegionAvail().x,48}) && takeStack) *takeStack=stack.id;
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("CORPSE_ITEM", &stack.id, sizeof(stack.id));
                    ImGui::TextUnformatted(GetItemDefinition(stack.type).name);
                    ImGui::EndDragDropSource();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (corpse->inventory.Items().empty() && !corpse->rifle) ImGui::TextUnformatted("수색 완료 · 남은 아이템이 없습니다.");
        ImGui::EndChild();
        ImGui::TextUnformatted(loot.Message().c_str());
        ImGui::EndDisabled();
        ImGui::End();
        ImGui::PopStyleColor(3);
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RaidHud::Shutdown()
{
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

namespace
{
    struct GearDrag {
        int type;
        int quantity;
        bool fromStash;
        StackId stack;
        int stashX = -1;
        int stashY = -1;
    };
    constexpr const char* GearPayload = "HIDEOUT_GEAR";
    constexpr int RifleGear = ItemTypeCount;
    struct CharacterPreview
    {
        CharacterVisual* visual;
        Player* character;
        Shader* shader;
        Camera camera;
        ImVec2 position;
        ImVec2 size;
        ImVec2 framebufferScale;
        bool rifle;
    };

    void DrawCharacterPreview(const ImDrawList*, const ImDrawCmd* command)
    {
        auto& preview = *static_cast<CharacterPreview*>(command->UserCallbackData);
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        const int x = static_cast<int>(std::floor(preview.position.x * preview.framebufferScale.x));
        const int width = static_cast<int>(std::ceil(preview.size.x * preview.framebufferScale.x));
        const int height = static_cast<int>(std::ceil(preview.size.y * preview.framebufferScale.y));
        const int y = viewport[1] + viewport[3] -
            static_cast<int>(std::ceil((preview.position.y + preview.size.y) * preview.framebufferScale.y));
        glViewport(x,y,width,height);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x,y,width,height);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        Renderer::PrepareFrame(*preview.shader,preview.camera,preview.size.x,preview.size.y);
        const float dt=std::clamp(std::isfinite(ImGui::GetIO().DeltaTime)?
            ImGui::GetIO().DeltaTime:0.0f,0.0f,0.1f);
        preview.visual->Update(dt,*preview.character,{false,preview.rifle,false,false,0,0,preview.rifle});
        preview.visual->Render(*preview.character,*preview.shader,preview.camera,preview.size.x,preview.size.y);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    }

    void GearIcon(ImDrawList* draw, ImVec2 p, int type, ImU32 color)
    {
        if (type == RifleGear) {
            draw->AddRectFilled({p.x+4,p.y+19},{p.x+56,p.y+27},color,2);
            draw->AddRectFilled({p.x+48,p.y+21},{p.x+72,p.y+24},color);
            draw->AddQuadFilled({p.x+5,p.y+21},{p.x+20,p.y+25},{p.x+15,p.y+35},{p.x,p.y+34},color);
            draw->AddRectFilled({p.x+30,p.y+27},{p.x+39,p.y+40},color,2);
        } else if (type == static_cast<int>(ItemType::Boots)) {
            for (float x : {p.x+10, p.x+36}) {
                draw->AddRectFilled({x,p.y+7},{x+15,p.y+33},color,2);
                draw->AddRectFilled({x,p.y+29},{x+25,p.y+40},color,3);
                draw->AddLine({x,p.y+41},{x+25,p.y+41},IM_COL32(20,22,25,255),3);
                for (float y=p.y+13;y<p.y+28;y+=5)
                    draw->AddLine({x+4,y},{x+12,y},IM_COL32(85,88,93,255),1);
            }
        } else if (IsClothing(static_cast<ItemType>(type))) {
            draw->AddRectFilled({p.x+18,p.y+8},{p.x+49,p.y+41},color,3);
            draw->AddLine({p.x+19,p.y+10},{p.x+7,p.y+25},color,9);
            draw->AddLine({p.x+48,p.y+10},{p.x+60,p.y+25},color,9);
        } else if (type == static_cast<int>(ItemType::Bandage)) {
            draw->AddRectFilled({p.x+12,p.y+6},{p.x+49,p.y+40},color,4);
            draw->AddRectFilled({p.x+27,p.y+12},{p.x+34,p.y+34},IM_COL32(80,33,29,255));
            draw->AddRectFilled({p.x+20,p.y+19},{p.x+41,p.y+26},IM_COL32(80,33,29,255));
        } else if (type == static_cast<int>(ItemType::Water)) {
            draw->AddRectFilled({p.x+24,p.y+3},{p.x+37,p.y+10},color,2);
            draw->AddRectFilled({p.x+19,p.y+11},{p.x+42,p.y+42},color,6);
            draw->AddRectFilled({p.x+20,p.y+22},{p.x+41,p.y+31},IM_COL32(40,62,73,255));
        } else if (type == static_cast<int>(ItemType::Backpack) || type == static_cast<int>(ItemType::Vest)) {
            draw->AddRectFilled({p.x+17,p.y+8},{p.x+47,p.y+42},color,5);
            draw->AddRectFilled({p.x+25,p.y+16},{p.x+39,p.y+27},IM_COL32(53,60,44,255),2);
            draw->AddLine({p.x+21,p.y+11},{p.x+14,p.y+30},color,4);
            draw->AddLine({p.x+43,p.y+11},{p.x+50,p.y+30},color,4);
        } else {
            for (int i=0;i<4;++i) {
                float x=p.x+12+i*11;
                draw->AddRectFilled({x,p.y+15},{x+7,p.y+39},color,1);
                draw->AddTriangleFilled({x,p.y+15},{x+7,p.y+15},{x+3.5f,p.y+5},color);
            }
        }
    }
}

bool RaidHud::RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader)
{
    if (!rendererReady) return false;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    const ImVec4 accent(.66f,.61f,.43f,1);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.045f,.05f,.047f,1));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.07f,.077f,.07f,1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.17f,.18f,.15f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.30f,.31f,.23f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.40f,.39f,.27f,1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.28f,.29f,.24f,1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.84f,.84f,.78f,1));
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, accent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,18));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1);
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("은신처", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    ImGui::TextColored(accent, "프로젝트 타르코프");
    ImGui::SameLine();
    ImGui::TextDisabled("/ 은신처");
    ImGui::SameLine();
    ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(),io.DisplaySize.x*.34f));
    const bool equipmentTabActive=!hideoutHealthTab;
    if (equipmentTabActive) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.38f,.37f,.29f,1));
    if (ImGui::SmallButton("장비")) hideoutHealthTab=false;
    if (equipmentTabActive) ImGui::PopStyleColor();
    ImGui::SameLine();
    const bool healthTabActive=hideoutHealthTab;
    if (healthTabActive) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.38f,.37f,.29f,1));
    if (ImGui::SmallButton("건강")) hideoutHealthTab=true;
    if (healthTabActive) ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::BeginDisabled();
    for (const char* tab : {"기술","지도","임무"}) { ImGui::SmallButton(tab); ImGui::SameLine(); }
    ImGui::EndDisabled();
    ImGui::NewLine();
    ImGui::Separator();
    if (ImGui::Button("게임 종료",{110,34}))
        glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
    ImGui::SameLine();
    ImGui::BeginDisabled(!hideout.ready || !Input::IsFocused());
    if (!hideout.rifle) ImGui::TextColored(accent,"비무장 출격");
    ImGui::BeginDisabled(!hideout.vitals.Alive());
    const bool deploy=ImGui::Button("레이드 출격",{150,34});
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("저장 / 재시도",{135,34})) hideout.Save();
    ImGui::SameLine();
    ImGui::TextUnformatted(hideout.message.c_str());
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(!hideout.ready || !Input::IsFocused());

    GearDrag pending{};
    bool transfer = false, toStash = false, equipPending = false;
    bool healPending = false;
    bool healDialogRequested = false;
    bool equipRifleByClick = false;
    BodyPart healTarget = BodyPart::Head;
    ItemType equipmentTarget = ItemType::Shirt;
    auto target = [&](bool stashTarget, int accepts, int stashX = -1, int stashY = -1) {
        if (!hideout.ready || !Input::IsFocused()) return;
        if (!ImGui::BeginDragDropTarget()) return;
        const auto* candidate = ImGui::GetDragDropPayload();
        if (candidate && candidate->IsDataType(GearPayload) && candidate->DataSize == sizeof(GearDrag)) {
            const auto data = *static_cast<const GearDrag*>(candidate->Data);
            if (data.fromStash && stashTarget && data.stashX >= 0 && data.stashY >= 0 &&
                stashX >= 0 && stashY >= 0) {
                if (const auto* payload = ImGui::AcceptDragDropPayload(GearPayload);
                    payload && payload->IsDelivery())
                    hideout.MoveStashItem(data.stashX, data.stashY, stashX, stashY);
            } else if (data.fromStash != stashTarget && data.type >= 0 && data.type <= RifleGear &&
                data.quantity > 0 && (accepts < 0 || data.type == accepts) &&
                (stashTarget || data.type != RifleGear || accepts == RifleGear)) {
                if (const auto* payload = ImGui::AcceptDragDropPayload(GearPayload)) {
                    if (payload->IsDelivery()) {
                        pending = data; transfer = true; toStash = stashTarget;
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    };
    auto equipmentDropTarget = [&](ItemType type) {
        if (!hideout.ready || !Input::IsFocused()) return;
        if (!ImGui::BeginDragDropTarget()) return;
        const auto* candidate=ImGui::GetDragDropPayload();
        if (candidate&&candidate->IsDataType(GearPayload)&&candidate->DataSize==sizeof(GearDrag)) {
            const auto data=*static_cast<const GearDrag*>(candidate->Data);
            if (data.type==static_cast<int>(type)&&data.quantity>0&&
                !(hideout.clothingMask&ClothingBit(type))) {
                if (const auto* payload=ImGui::AcceptDragDropPayload(GearPayload);payload&&payload->IsDelivery()) {
                    pending=data;
                    equipmentTarget=type;
                    equipPending=true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    };
    auto card = [&](int id, const char* name, int type, int count, bool sourceStash, StackId stack, float width, float height) {
        ImGui::PushID(id);
        const auto start = ImGui::GetCursorScreenPos();
        ImGui::Button("##gear", {width,height});
        target(sourceStash,sourceStash || type != RifleGear ? -1 : RifleGear);
        if (count > 0 && ImGui::BeginDragDropSource()) {
            GearDrag data{type,sourceStash && type<RifleGear ? (std::min)(count,GetItemDefinition(static_cast<ItemType>(type)).maxStack) : count,sourceStash,stack};
            ImGui::SetDragDropPayload(GearPayload,&data,sizeof(data));
            ImGui::Text("%s x%d",name,type==RifleGear?1:data.quantity);
            ImGui::EndDragDropSource();
        }
        auto* draw = ImGui::GetWindowDrawList();
        if (count > 0) GearIcon(draw,{start.x+8,start.y+5},type,
            type==1 ? IM_COL32(122,155,168,255) : IM_COL32(182,171,130,255));
        draw->AddText({start.x+8,start.y+height-35},IM_COL32(216,215,195,255),name);
        const std::string amount = count ? "x"+std::to_string(count) : "비어 있음";
        draw->AddText({start.x+8,start.y+height-17},IM_COL32(155,155,138,255),amount.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(name);
            ImGui::TextUnformatted(sourceStash ? "출전 장비로 끌어 옮기세요." : "보관 영역으로 끌어 옮기세요.");
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    };

    const bool wide=ImGui::GetContentRegionAvail().x>=1050;
    const float available=ImGui::GetContentRegionAvail().y;
    const float panelHeight=(std::max)(180.f,wide||hideoutHealthTab?available:(available-8.f)*.5f);
    Player previewCharacter=character;
    previewCharacter.position={0,0,0};
    previewCharacter.rotation.y=hideoutPreviewRotation;
    previewCharacter.clothingMask=hideout.clothingMask;
    CharacterPreview preview{&visual,&previewCharacter,&shader,{}, {}, {}, io.DisplayFramebufferScale,hideout.rifle};
    preview.camera.position={0,1.0f,3.4f};
    preview.camera.front={0,0,-1};
    preview.camera.up={0,1,0};
    preview.camera.fieldOfView=38.f;

    auto drawCharacterPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const float panelWidth=ImGui::GetContentRegionAvail().x;
        const float slotWidth=(std::min)(208.f,(std::max)(100.f,(panelWidth-140.f)*.5f));
        const float modelWidth=(std::max)(1.f,panelWidth-slotWidth*2-18.f);
        const float modelHeight=(std::max)(245.f,(std::min)(480.f,panelHeight-24.f));
        const ImVec2 modelPosition(origin.x+slotWidth+9.f,origin.y+8.f);
        const ImVec2 modelSize(modelWidth,modelHeight);
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(modelPosition,{modelPosition.x+modelSize.x,modelPosition.y+modelSize.y},
            IM_COL32(25,29,26,235),3.f);
        for (float x=modelPosition.x+20;x<modelPosition.x+modelSize.x;x+=28)
            draw->AddLine({x,modelPosition.y},{x,modelPosition.y+modelSize.y},IM_COL32(55,61,52,65));
        preview.position=modelPosition;
        preview.size=modelSize;
        draw->AddCallback(DrawCharacterPreview,&preview);
        draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        ImGui::SetCursorScreenPos(modelPosition);
        ImGui::InvisibleButton("CharacterRotationTarget",modelSize);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            hideoutPreviewRotation=std::fmod(hideoutPreviewRotation+90.f,360.f);

        const ItemType leftTypes[]{ItemType::Helmet,ItemType::Shirt,ItemType::UnderShirt,ItemType::Backpack};
        const char* leftNames[]{"머리","상의","브라","배낭"};
        const ItemType rightTypes[]{ItemType::Vest,ItemType::Trousers,ItemType::Underpants,ItemType::Boots};
        const char* rightNames[]{"전술조끼","하의","팬티","신발"};
        auto slot=[&](int id,ItemType type,const char* name,ImVec2 position) {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(position);
            const bool worn=(hideout.clothingMask&ClothingBit(type))!=0;
            const bool available=hideout.loadout.Count(type)>0||hideout.stash[static_cast<int>(type)]>0;
            const float slotHeight=124.f;
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.14f,.16f,.14f,.42f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.28f,.29f,.23f,.55f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.38f,.37f,.29f,.65f));
            ImGui::Button("##equipment-slot",{slotWidth,slotHeight});
            ImGui::PopStyleColor(3);
            equipmentDropTarget(type);
            if (ImGui::IsItemClicked() && (worn||available))
                hideout.ToggleClothing(type);
            auto* slotDraw=ImGui::GetWindowDrawList();
            const ImVec2 low(position.x+1,position.y+1), high(position.x+slotWidth-1,position.y+slotHeight-1);
            slotDraw->AddRect(low,high,worn?IM_COL32(188,170,108,230):IM_COL32(116,121,103,210),2.f,0,1.5f);
            slotDraw->AddText({position.x+6,position.y+7},IM_COL32(208,207,188,255),name);
            const char* value=worn?GetItemDefinition(type).name:"비어 있음";
            slotDraw->AddText({position.x+6,position.y+70},worn?IM_COL32(219,205,146,255):IM_COL32(137,143,128,255),value);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s 슬롯",name);
                ImGui::TextUnformatted(worn?"클릭하면 창고에 보관합니다.":
                    available?"해당 장비를 끌어 놓거나 클릭해 착용합니다.":"창고에서 장비를 끌어 놓으세요.");
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        };
        const float step=132.f;
        for (int i=0;i<4;++i)
            slot(600+i,leftTypes[i],leftNames[i],{origin.x,modelPosition.y+8+i*step});
        for (int i=0;i<4;++i)
            slot(610+i,rightTypes[i],rightNames[i],{origin.x+panelWidth-slotWidth,modelPosition.y+44+i*step});
        ImGui::EndChild();
    };

    auto drawHealthPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const float panelWidth=ImGui::GetContentRegionAvail().x;
        const float scale=(std::min)(.72f,(panelHeight-20.f)/260.f);
        const ImVec2 bodyOrigin(origin.x+8.f,origin.y+8.f);
        DrawBodyMannequin(hideout.vitals,bodyOrigin,scale);
        const auto rectangles=BodyPartRects(bodyOrigin,scale);
        for (int i=0;i<BodyPartCount;++i) {
            const auto& rectangle=rectangles[static_cast<std::size_t>(i)];
            ImGui::SetCursorScreenPos({rectangle.x,rectangle.y});
            ImGui::PushID(700+i);
            ImGui::InvisibleButton("HealDrop",{rectangle.z-rectangle.x,rectangle.w-rectangle.y});
            if (ImGui::BeginDragDropTarget()) {
                const auto* candidate=ImGui::GetDragDropPayload();
                if (candidate&&candidate->IsDataType(GearPayload)&&candidate->DataSize==sizeof(GearDrag)) {
                    const auto data=*static_cast<const GearDrag*>(candidate->Data);
                    if (data.fromStash&&data.type==static_cast<int>(ItemType::Bandage)) {
                        if (const auto* payload=ImGui::AcceptDragDropPayload(GearPayload);payload&&payload->IsDelivery()) {
                            healPending=true;
                            healTarget=static_cast<BodyPart>(i);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s: %.0f / 100",BodyPartNames[i],hideout.vitals.BodyHealth(static_cast<BodyPart>(i)));
                ImGui::TextUnformatted("붕대를 끌어 놓아 치료");
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
        const ImVec2 modelPosition(origin.x+panelWidth*.43f,origin.y+8.f);
        const ImVec2 modelSize(panelWidth*.55f,panelHeight-24.f);
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(modelPosition,{modelPosition.x+modelSize.x,modelPosition.y+modelSize.y},
            IM_COL32(25,29,26,235),3.f);
        preview.position=modelPosition;
        preview.size=modelSize;
        draw->AddCallback(DrawCharacterPreview,&preview);
        draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        ImGui::EndChild();
    };

    auto drawLoadoutPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        ImGui::Separator();
        ImGui::TextUnformatted("주무기");
        card(100,"소총",RifleGear,hideout.rifle?1:0,false,0,ImGui::GetContentRegionAvail().x,82);
        ImGui::Text("탄창   %d / 30",hideout.magazine);
        ImGui::SameLine();
        ImGui::BeginDisabled(!hideout.rifle||hideout.magazine>=30||hideout.stash[static_cast<int>(ItemType::Ammo)]==0);
        if (ImGui::SmallButton("탄창 장전")) hideout.LoadMagazine();
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::Text("휴대 슬롯   %d / %d   |   무게 %.1f kg",
            static_cast<int>(hideout.loadout.Items().size()),hideout.loadout.SlotCapacity(),hideout.loadout.TotalWeight());
        const auto& items=hideout.loadout.Items();
        const int columns=(std::max)(1,static_cast<int>(ImGui::GetContentRegionAvail().x/210));
        const float gap=6.f;
        const float cell=(ImGui::GetContentRegionAvail().x-(columns-1)*gap)/columns;
        for (int i=0;i<hideout.loadout.SlotCapacity();++i) {
            if (i%columns) ImGui::SameLine(0,gap);
            if (i<static_cast<int>(items.size())) {
                const auto& stack=items[i];
                card(200+i,GetItemDefinition(stack.type).name,static_cast<int>(stack.type),stack.quantity,false,stack.id,cell,82);
            } else card(200+i,"빈 슬롯",0,0,false,0,cell,82);
        }
        ImGui::EndChild();
    };

    auto drawStashPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        if (ImGui::SmallButton("창고 자동정리"))
            hideout.AutoArrangeStash();
        constexpr int stashColumns=20, stashRows=4;
        const float gap=2.f;
        const ImVec2 panelAvailable=ImGui::GetContentRegionAvail();
        const float stashWidth=panelAvailable.x;
        const float scrollHeight=(std::max)(64.f,panelAvailable.y);
        ImGui::BeginChild("StashGridScroll",{stashWidth,scrollHeight},
            true,ImGuiWindowFlags_AlwaysVerticalScrollbar);
        const ImVec2 gridViewport=ImGui::GetContentRegionAvail();
        const float widthCell=(gridViewport.x-(stashRows-1)*gap)/stashRows;
        const float heightCell=(scrollHeight-8.f*gap)/7.f;
        const float cell=(std::max)(1.f,(std::min)((std::min)(widthCell,heightCell),96.f));
        const ImVec2 gridOrigin=ImGui::GetCursorScreenPos();
        const auto placements=hideout.StashLayout();
        std::array<std::array<bool,stashRows>,stashColumns> occupied{};
        for (const auto& item : placements)
            for (int x=0;x<item.width;++x)
                for (int y=0;y<item.height;++y)
                    occupied[static_cast<std::size_t>(item.x+x)][static_cast<std::size_t>(item.y+y)]=true;
        auto* draw=ImGui::GetWindowDrawList();
        for (int x=0;x<stashColumns;++x) for (int y=0;y<stashRows;++y) {
            const ImVec2 low(gridOrigin.x+y*(cell+gap),gridOrigin.y+x*(cell+gap));
            const ImU32 tileColor=((x+y)&1) ? IM_COL32(38,43,37,255) : IM_COL32(29,34,30,255);
            draw->AddRectFilled(low,{low.x+cell,low.y+cell},tileColor);
            draw->AddRect(low,{low.x+cell,low.y+cell},IM_COL32(102,111,91,210),0.f,0,1.f);
            if (!occupied[static_cast<std::size_t>(x)][static_cast<std::size_t>(y)]) {
                ImGui::SetCursorScreenPos(low);
                ImGui::PushID(x*stashRows+y);
                ImGui::InvisibleButton("EmptyStashSlot",{cell,cell});
                target(true,-1,x,y);
                ImGui::PopID();
            }
        }
        for (std::size_t i=0;i<placements.size();++i) {
            const auto& item=placements[i];
            const ImVec2 low(gridOrigin.x+item.y*(cell+gap),gridOrigin.y+item.x*(cell+gap));
            const ImVec2 size(item.height*cell+(item.height-1)*gap,item.width*cell+(item.width-1)*gap);
            const ImU32 colors[]{
                IM_COL32(135,68,58,255), IM_COL32(63,112,137,255), IM_COL32(152,132,71,255),
                IM_COL32(91,113,77,255), IM_COL32(84,95,70,255), IM_COL32(121,97,83,255),
                IM_COL32(111,86,82,255), IM_COL32(91,104,80,255), IM_COL32(122,123,111,255),
                IM_COL32(72,99,76,255), IM_COL32(96,111,86,255)
            };
            const ImU32 color=colors[item.type];
            draw->AddRectFilled(low,{low.x+size.x,low.y+size.y},color,2);
            const ImU32 iconColor=IM_COL32(225,220,197,215);
            if (item.type==static_cast<int>(ItemType::Bandage)) {
                draw->AddRectFilled({low.x+size.x*.42f,low.y+size.y*.2f},
                    {low.x+size.x*.58f,low.y+size.y*.8f},iconColor,1);
                draw->AddRectFilled({low.x+size.x*.2f,low.y+size.y*.42f},
                    {low.x+size.x*.8f,low.y+size.y*.58f},iconColor,1);
            } else if (item.type==static_cast<int>(ItemType::Water)) {
                draw->AddCircleFilled({low.x+size.x*.5f,low.y+size.y*.55f},
                    (std::min)(size.x,size.y)*.22f,iconColor,12);
            } else if (item.type==RifleGear) {
                draw->AddLine({low.x+size.x*.12f,low.y+size.y*.75f},
                    {low.x+size.x*.88f,low.y+size.y*.25f},iconColor,(std::max)(1.f,cell*.12f));
            } else {
                draw->AddRectFilled({low.x+size.x*.27f,low.y+size.y*.27f},
                    {low.x+size.x*.73f,low.y+size.y*.73f},iconColor,2);
            }
            ImGui::SetCursorScreenPos(low);
            ImGui::PushID(static_cast<int>(i));
            ImGui::InvisibleButton("StashItem",{size.x,size.y});
            const bool clicked=ImGui::IsItemClicked(ImGuiMouseButton_Left);
            target(true,-1);
            if (ImGui::BeginDragDropSource()) {
                GearDrag data{item.type,1,true,0,item.x,item.y};
                ImGui::SetDragDropPayload(GearPayload,&data,sizeof(data));
                const char* name=item.type==RifleGear?"소총":GetItemDefinition(static_cast<ItemType>(item.type)).name;
                ImGui::Text("%s",name);
                ImGui::EndDragDropSource();
            }
            if (clicked) {
                if (hideoutHealthTab && item.type==static_cast<int>(ItemType::Bandage)) {
                    healDialogRequested=true;
                } else if (item.type==RifleGear) {
                    equipRifleByClick=true;
                } else if (IsClothing(static_cast<ItemType>(item.type))) {
                    pending={item.type,1,true,0};
                    equipmentTarget=static_cast<ItemType>(item.type);
                    equipPending=true;
                } else {
                    pending={item.type,1,true,0};
                    transfer=true;
                    toStash=false;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(item.type==RifleGear?"소총":GetItemDefinition(static_cast<ItemType>(item.type)).name);
                ImGui::Text("창고 수량: %d",item.type==RifleGear?hideout.rifles:hideout.stash[item.type]);
                ImGui::TextUnformatted("빈 슬롯으로 끌어 위치를 옮길 수 있습니다.");
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            if (equipRifleByClick) {
                if (!hideout.rifle) hideout.ToggleRifle();
                else hideout.message="이미 소총을 장착했습니다.";
                equipRifleByClick=false;
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();
    };

    if (hideoutHealthTab) {
        if (ImGui::BeginTable("HideoutHealthLayout",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("건강 상태",ImGuiTableColumnFlags_WidthStretch,.9f);
            ImGui::TableSetupColumn("보관품",ImGuiTableColumnFlags_WidthStretch,1.1f);
            ImGui::TableNextColumn();
            drawHealthPanel("HealthCharacterPanel");
            ImGui::TableNextColumn();
            drawStashPanel("HealthStashPanel");
            ImGui::EndTable();
        }
    } else if (ImGui::BeginTable(wide?"HideoutLayout":"HideoutLayoutNarrow",wide?3:2,
        ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)) {
        if (wide) {
            ImGui::TableSetupColumn("캐릭터",ImGuiTableColumnFlags_WidthStretch,1.3f);
            ImGui::TableSetupColumn("인벤토리",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableSetupColumn("창고",ImGuiTableColumnFlags_WidthStretch,1.2f);
            ImGui::TableNextColumn();
            drawCharacterPanel("CharacterPanel");
            ImGui::TableNextColumn();
            drawLoadoutPanel("LoadoutPanel");
            ImGui::TableNextColumn();
            drawStashPanel("StashPanel");
        } else {
            ImGui::TableSetupColumn("캐릭터와 인벤토리",ImGuiTableColumnFlags_WidthStretch,1.1f);
            ImGui::TableSetupColumn("창고",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableNextColumn();
            drawCharacterPanel("CharacterPanelNarrow");
            drawLoadoutPanel("LoadoutPanelNarrow");
            ImGui::TableNextColumn();
            drawStashPanel("StashPanelNarrow");
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if (healDialogRequested) ImGui::OpenPopup("치료 부위 선택##Hideout");
    if (ImGui::BeginPopupModal("치료 부위 선택##Hideout",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("치료할 부위를 선택하세요. (창고 붕대 %d개)",
            hideout.stash[static_cast<std::size_t>(ItemType::Bandage)]);
        for (int i=0;i<BodyPartCount;++i) {
            const auto part=static_cast<BodyPart>(i);
            ImGui::PushID(800+i);
            ImGui::BeginDisabled(hideout.vitals.BodyHealth(part)>=100.f);
            if (ImGui::Button(BodyPartNames[i],{145,32})) {
                healPending=true;
                healTarget=part;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text("%.0f / 100",hideout.vitals.BodyHealth(part));
            ImGui::PopID();
        }
        if (ImGui::Button("취소",{145,32})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    // Mutate only after every source item has been drawn, never while iterating inventory.
    if (healPending) {
        hideout.HealBodyPart(healTarget);
    } else if (equipPending) {
        hideout.ToggleClothing(equipmentTarget,pending.fromStash);
    } else if (transfer) {
        if (pending.type==RifleGear) {
            if (pending.fromStash ? !hideout.rifle && hideout.rifles>0 : hideout.rifle) hideout.ToggleRifle();
            else hideout.message="무기 칸에 이미 장비가 있습니다.";
        } else if (pending.quantity>0) {
            if (!toStash) hideout.Transfer(static_cast<ItemType>(pending.type),pending.quantity,true);
            else hideout.StoreStack(pending.stack);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(8);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return deploy;
}

bool RaidHud::RenderExtractionSummary(const RaidSummary& summary, Player& character, CharacterVisual& visual, Shader& shader)
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
    ImGui::TextColored(accent, "탈출 성공");
    ImGui::SameLine();
    ImGui::TextDisabled("/ 산업지대 레이드 결과");
    ImGui::Separator();

    ImGui::BeginChild("결과 캐릭터", {0, io.DisplaySize.y * 0.43f}, true);
    const ImVec2 modelPosition = ImGui::GetCursorScreenPos();
    const float modelWidth = ImGui::GetContentRegionAvail().x;
    const float modelHeight = (std::max)(180.0f, ImGui::GetContentRegionAvail().y - 8.0f);
    const ImVec2 modelSize(modelWidth, modelHeight);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(modelPosition,
        {modelPosition.x + modelSize.x, modelPosition.y + modelSize.y},
        IM_COL32(25, 29, 26, 235), 3.0f);

    CharacterPreview preview{&visual, &character, &shader, {}, modelPosition, modelSize, io.DisplayFramebufferScale, summary.recoveredRifle};
    preview.camera.position = {0, 1.0f, 3.4f};
    preview.camera.front = {0, 0, -1};
    preview.camera.up = {0, 1, 0};
    preview.camera.fieldOfView = 38.0f;
    draw->AddCallback(DrawCharacterPreview, &preview);
    draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    ImGui::Dummy(modelSize);
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
    ImGui::BeginChild("획득 장비", {0, detailsHeight}, true);
    ImGui::TextColored(accent, "획득 장비 (기존 장비 제외)");
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
