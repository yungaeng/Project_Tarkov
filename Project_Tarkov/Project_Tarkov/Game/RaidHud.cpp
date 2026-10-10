#include "RaidHud.h"
#include "RaidStatus.h"
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
#include <string_view>
#include <Windows.h>

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

void RaidHud::Render(const Player& player, const LootSystem& loot, const CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid)
{
    if (!rendererReady) return;
    const auto& inventory = player.GetInventory();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    const ImGuiWindowFlags overlay = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowPos(ImVec2(16, 16));
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::Begin("조작 안내", nullptr, overlay);
    ImGui::TextUnformatted("WASD 이동 | I 인벤토리 | Q 종료");
    ImGui::Text("인벤토리: %d / %d칸 | %.2f kg", static_cast<int>(inventory.Items().size()),
        inventory.SlotCapacity(), inventory.TotalWeight());
    ImGui::Text("체력 %.0f/100 | 수분 %.0f/100 | 스태미나 %.0f/100", player.vitals.Health(), player.vitals.Hydration(), player.vitals.Stamina());
    ImGui::Text("소총: %s | %d/30 | 예비 탄약 %d | 남은 적 %d", combat.Equipped() ? "장착 중" : combat.HasRifle() ? "수납 중" : "미보유", combat.Magazine(), inventory.Count(ItemType::Ammo), combat.LivingEnemies());
    ImGui::TextUnformatted("1 장착/수납 | 마우스 왼쪽 사격 | 오른쪽 조준 | R 재장전");
    if (combat.ReloadTime() > 0) ImGui::Text("재장전 중 %.1f초", combat.ReloadTime());
    if (!combat.Message().empty()) ImGui::TextUnformatted(combat.Message().c_str());
    if (!player.vitals.Alive()) ImGui::TextUnformatted("사망했습니다 - Q키로 종료");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16, 16), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::Begin("레이드", nullptr, raid.Finished() ? (overlay & ~ImGuiWindowFlags_NoInputs) : overlay);
    const int seconds = static_cast<int>(std::ceil(raid.remaining));
    ImGui::Text("산업지대 | %02d:%02d", seconds / 60, seconds % 60);
    ImGui::Text("위치 X %.0f / Z %.0f (북쪽 = -Z)", player.position.x, player.position.z);
    if (raid.phase == RaidPhase::Active) {
        const auto& exit = IndustrialZone::Exits[raid.assignedExit];
        ImGui::Text("지정 탈출구: %s | %.0fm", exit.name, raid.exitDistance);
        ImGui::Text("탈출구 X %.0f / Z %.0f | 초록색 사각형 안에서 대기", exit.position.x, exit.position.z);
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

    if (raid.phase == RaidPhase::Active && !inventoryOpen)
    {
        const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        if (combat.Aiming() && player.vitals.Alive() && Input::IsFocused()) {
            auto* draw = ImGui::GetForegroundDrawList();
            const ImU32 color = loot.Target() ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 255, 255, 220);
            draw->AddLine(ImVec2(center.x - 5, center.y), ImVec2(center.x + 5, center.y), color);
            draw->AddLine(ImVec2(center.x, center.y - 5), ImVec2(center.x, center.y + 5), color);
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
    else if (raid.phase == RaidPhase::Active && inventoryOpen)
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x < 560 ? io.DisplaySize.x : 560,
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
                stack->type == ItemType::Bandage ? "효과: 체력 +20" : stack->type == ItemType::Water ? "효과: 수분 +30" : "탄약: 인벤토리를 닫고 R키로 재장전하세요");
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

namespace
{
    struct GearDrag { int type; int quantity; bool fromStash; StackId stack; };
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
        preview.visual->Update(0,*preview.character,{false,preview.rifle,false,false,0,0,preview.rifle});
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
    ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(),io.DisplaySize.x*.39f));
    for (const char* tab : {"전체","장비","건강","기술","지도","임무"}) {
        if (std::string_view(tab)=="장비") ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.38f,.37f,.29f,1));
        else ImGui::BeginDisabled();
        ImGui::SmallButton(tab);
        if (std::string_view(tab)=="장비") ImGui::PopStyleColor(); else ImGui::EndDisabled();
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::TextDisabled("장비 준비   /   산업지대   /   레이드 실패 시 휴대한 장비를 잃습니다");
    ImGui::Separator();
    ImGui::BeginDisabled(!hideout.ready || !Input::IsFocused());

    GearDrag pending{};
    bool transfer = false, toStash = false, equipPending = false;
    ItemType equipmentTarget = ItemType::Shirt;
    auto target = [&](bool stashTarget, int accepts) {
        if (!hideout.ready || !Input::IsFocused()) return;
        // Reject incompatible targets before accepting: acceptance consumes a delivered payload.
        const auto* candidate = ImGui::GetDragDropPayload();
        if (!candidate || !candidate->IsDataType(GearPayload) || candidate->DataSize != sizeof(GearDrag)) return;
        const auto data = *static_cast<const GearDrag*>(candidate->Data);
        if (data.fromStash == stashTarget || data.type < 0 || data.type > RifleGear || data.quantity <= 0 ||
            (accepts >= 0 && data.type != accepts) || (!stashTarget && data.type == RifleGear && accepts != RifleGear)) return;
        if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload = ImGui::AcceptDragDropPayload(GearPayload)) {
                if (payload->IsDelivery()) {
                    pending = data; transfer = true; toStash = stashTarget;
                }
            }
            ImGui::EndDragDropTarget();
        }
    };
    auto equipmentDropTarget = [&](ItemType type) {
        if (!hideout.ready || !Input::IsFocused()) return;
        const auto* candidate=ImGui::GetDragDropPayload();
        if (!candidate||!candidate->IsDataType(GearPayload)||candidate->DataSize!=sizeof(GearDrag)) return;
        const auto data=*static_cast<const GearDrag*>(candidate->Data);
        if (data.type!=static_cast<int>(type)||data.quantity<=0||
            (hideout.clothingMask&ClothingBit(type))) return;
        if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload=ImGui::AcceptDragDropPayload(GearPayload);payload&&payload->IsDelivery()) {
                pending=data;
                equipmentTarget=type;
                equipPending=true;
            }
            ImGui::EndDragDropTarget();
        }
    };
    auto card = [&](int id, const char* name, int type, int count, bool sourceStash, StackId stack, float width, float height) {
        ImGui::PushID(id);
        const auto start = ImGui::GetCursorScreenPos();
        ImGui::Button("##gear", {width,height});
        auto* draw = ImGui::GetWindowDrawList();
        if (count > 0) GearIcon(draw,{start.x+8,start.y+5},type,
            type==1 ? IM_COL32(122,155,168,255) : IM_COL32(182,171,130,255));
        draw->AddText({start.x+8,start.y+height-35},IM_COL32(216,215,195,255),name);
        const std::string amount = count ? "x"+std::to_string(count) : "비어 있음";
        draw->AddText({start.x+8,start.y+height-17},IM_COL32(155,155,138,255),amount.c_str());
        if (count > 0 && ImGui::BeginDragDropSource()) {
            GearDrag data{type,sourceStash && type<RifleGear ? (std::min)(count,GetItemDefinition(static_cast<ItemType>(type)).maxStack) : count,sourceStash,stack};
            ImGui::SetDragDropPayload(GearPayload,&data,sizeof(data));
            ImGui::Text("%s x%d",name,type==RifleGear?1:data.quantity);
            ImGui::EndDragDropSource();
        }
        target(sourceStash,sourceStash ? -1 : type);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(name);
            ImGui::TextUnformatted(sourceStash ? "캐릭터 인벤토리로 끌어 옮기세요." : "창고로 끌어 옮겨 보관하세요.");
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    };

    const float available=ImGui::GetContentRegionAvail().y;
    const float panelHeight=(std::max)(260.f,(std::min)(available-100.f,io.DisplaySize.y-300.f));
    const bool wide=ImGui::GetContentRegionAvail().x>=1050;
    Player previewCharacter=character;
    previewCharacter.position={0,0,0};
    previewCharacter.rotation.y=0.f;
    previewCharacter.clothingMask=hideout.clothingMask;
    CharacterPreview preview{&visual,&previewCharacter,&shader,{}, {}, {}, io.DisplayFramebufferScale,hideout.rifle};
    preview.camera.position={0,1.0f,3.4f};
    preview.camera.front={0,0,-1};
    preview.camera.up={0,1,0};
    preview.camera.fieldOfView=38.f;

    auto drawCharacterPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        ImGui::TextColored(accent,"캐릭터 장비");
        ImGui::TextDisabled("부위 슬롯을 눌러 착용/해제하거나 아이템을 끌어 놓으세요");
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const float panelWidth=ImGui::GetContentRegionAvail().x;
        const float slotWidth=(std::min)(104.f,(panelWidth-126.f)*.5f);
        const float modelWidth=(std::max)(116.f,panelWidth-slotWidth*2-18.f);
        const float modelHeight=(std::max)(245.f,(std::min)(340.f,panelHeight-210.f));
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

        const ItemType leftTypes[]{ItemType::Helmet,ItemType::Shirt,ItemType::UnderShirt,ItemType::Backpack};
        const char* leftNames[]{"머리","상의","내의","배낭"};
        const ItemType rightTypes[]{ItemType::Vest,ItemType::Trousers,ItemType::Underpants};
        const char* rightNames[]{"전술조끼","하의","하의 내의"};
        auto slot=[&](int id,ItemType type,const char* name,ImVec2 position) {
            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(position);
            const bool worn=(hideout.clothingMask&ClothingBit(type))!=0;
            const bool available=hideout.loadout.Count(type)>0||hideout.stash[static_cast<int>(type)]>0;
            ImGui::Button("##equipment-slot",{slotWidth,62.f});
            if (ImGui::IsItemClicked() && (worn||available))
                hideout.ToggleClothing(type);
            auto* slotDraw=ImGui::GetWindowDrawList();
            const ImVec2 low(position.x+1,position.y+1), high(position.x+slotWidth-1,position.y+61);
            slotDraw->AddRect(low,high,worn?IM_COL32(188,170,108,230):IM_COL32(116,121,103,210),2.f,0,1.5f);
            slotDraw->AddText({position.x+6,position.y+7},IM_COL32(208,207,188,255),name);
            const char* value=worn?GetItemDefinition(type).name:"비어 있음";
            slotDraw->AddText({position.x+6,position.y+34},worn?IM_COL32(219,205,146,255):IM_COL32(137,143,128,255),value);
            equipmentDropTarget(type);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s 슬롯",name);
                ImGui::TextUnformatted(worn?"클릭하면 창고에 보관합니다.":
                    available?"해당 장비를 끌어 놓거나 클릭해 착용합니다.":"창고에서 장비를 끌어 놓으세요.");
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        };
        const float step=70.f;
        for (int i=0;i<4;++i)
            slot(600+i,leftTypes[i],leftNames[i],{origin.x,modelPosition.y+8+i*step});
        for (int i=0;i<3;++i)
            slot(610+i,rightTypes[i],rightNames[i],{origin.x+panelWidth-slotWidth,modelPosition.y+44+i*step});
        ImGui::SetCursorScreenPos({origin.x,modelPosition.y+modelHeight+12.f});
        ImGui::TextWrapped("장착 상태는 캐릭터 외형과 휴대 공간에 반영되며 자동 저장됩니다.");
        ImGui::EndChild();
    };

    auto drawLoadoutPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        ImGui::TextColored(accent,"캐릭터 인벤토리");
        ImGui::TextDisabled("장착 아이템과 소모품을 함께 보관합니다");
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
        const int columns=(std::max)(1,static_cast<int>(ImGui::GetContentRegionAvail().x/105));
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
        target(false,-1);
    };

    auto drawStashPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        ImGui::TextColored(accent,"창고");
        ImGui::TextDisabled("안전 보관  /  보관품은 레이드 손실과 관계없이 유지됩니다");
        ImGui::Separator();
        const float stashWidth=ImGui::GetContentRegionAvail().x;
        const int stashColumns=(std::max)(1,static_cast<int>(stashWidth/125));
        const float gap=6.f;
        const float cell=(stashWidth-(stashColumns-1)*gap)/stashColumns;
        card(300,"소총",RifleGear,hideout.rifles,true,0,cell,82);
        if (stashColumns>1) ImGui::SameLine(0,gap);
        for (int i=0;i<ItemTypeCount;++i) {
            if ((i+1)%stashColumns) ImGui::SameLine(0,gap);
            card(301+i,GetItemDefinition(static_cast<ItemType>(i)).name,i,hideout.stash[i],true,0,cell,82);
        }
        ImGui::Dummy({0,8});
        ImGui::Button("여기에 놓아 창고에 보관",{stashWidth,56});
        target(true,-1);
        ImGui::EndChild();
        target(true,-1);
    };

    if (ImGui::BeginTable(wide?"HideoutLayout":"HideoutLayoutNarrow",wide?3:2,
        ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)) {
        if (wide) {
            ImGui::TableSetupColumn("캐릭터",ImGuiTableColumnFlags_WidthStretch,.82f);
            ImGui::TableSetupColumn("인벤토리",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableSetupColumn("창고",ImGuiTableColumnFlags_WidthStretch,1.45f);
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
    // Mutate only after every source item has been drawn, never while iterating inventory.
    if (equipPending) {
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
    ImGui::Separator();
    ImGui::Text("출격 지점   ");
    ImGui::SameLine();
    ImGui::RadioButton("남서쪽  →  북동쪽",&hideout.spawn,0);
    ImGui::SameLine();
    ImGui::RadioButton("북서쪽  →  남동쪽",&hideout.spawn,1);
    if (!hideout.rifle) ImGui::SameLine();
    if (!hideout.rifle) ImGui::TextColored(accent,"비무장");
    const bool deploy=ImGui::Button("레이드 출격",{220,44});
    ImGui::SameLine();
    if (ImGui::Button("저장 / 재시도",{150,44})) hideout.Save();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextWrapped("%s",hideout.message.c_str());
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(8);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return deploy;
}
