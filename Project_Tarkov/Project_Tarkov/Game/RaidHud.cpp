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
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>

RaidHud::~RaidHud() { Shutdown(); }

void RaidHud::Init(GLFWwindow* window)
{
    Shutdown();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    contextReady = true;
    ImGui::GetIO().IniFilename = nullptr;
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
    ImGui::Begin("Controls", nullptr, overlay);
    ImGui::TextUnformatted("WASD Move | F Pick up | I Inventory | Q Quit");
    ImGui::Text("Inventory: %d / %d slots | %.2f kg", static_cast<int>(inventory.Items().size()),
        static_cast<int>(Inventory::Capacity), inventory.TotalWeight());
    ImGui::Text("HP %.0f/100 | Water %.0f/100 | Stamina %.0f/100", player.vitals.Health(), player.vitals.Hydration(), player.vitals.Stamina());
    ImGui::Text("Rifle: %s | %d/30 | Reserve %d | Enemies %d", combat.Equipped() ? "Equipped" : combat.HasRifle() ? "Holstered" : "Not carried", combat.Magazine(), inventory.Count(ItemType::Ammo), combat.LivingEnemies());
    ImGui::TextUnformatted("1 Equip/Holster | LMB Fire | RMB Aim | R Reload");
    if (combat.ReloadTime() > 0) ImGui::Text("Reloading %.1fs", combat.ReloadTime());
    if (!combat.Message().empty()) ImGui::TextUnformatted(combat.Message().c_str());
    if (!player.vitals.Alive()) ImGui::TextUnformatted("YOU DIED - Press Q to quit");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16, 16), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::Begin("Raid", nullptr, raid.Finished() ? (overlay & ~ImGuiWindowFlags_NoInputs) : overlay);
    const int seconds = static_cast<int>(std::ceil(raid.remaining));
    ImGui::Text("Industrial Zone | %02d:%02d", seconds / 60, seconds % 60);
    ImGui::Text("Position X %.0f / Z %.0f (North = -Z)", player.position.x, player.position.z);
    if (raid.phase == RaidPhase::Active) {
        const auto& exit = IndustrialZone::Exits[raid.assignedExit];
        ImGui::Text("Assigned: %s | %.0fm", exit.name, raid.exitDistance);
        ImGui::Text("Exit X %.0f / Z %.0f | Stay inside green square", exit.position.x, exit.position.z);
        if (raid.extraction > 0)
            ImGui::Text("Extracting: %.1fs", RaidConfig::DefaultExtractTime - raid.extraction);
    }
    else {
        ImGui::TextUnformatted(raid.phase == RaidPhase::Extracted ? "SURVIVED" :
            raid.phase == RaidPhase::Dead ? "KILLED IN ACTION - equipment lost" : "MIA - equipment lost");
        ImGui::Text("Recovered: %d stacks | %.2f kg", static_cast<int>(raid.recovered.Items().size()), raid.recovered.TotalWeight());
        for (const auto& stack : raid.recovered.Items())
            ImGui::Text("%s x%d", GetItemDefinition(stack.type).name, stack.quantity);
        ImGui::TextUnformatted(raid.saveMessage.c_str());
        ImGui::BeginDisabled(!Input::IsFocused());
        if (ImGui::Button("Return to Hideout")) raid.returnToHideout = true;
        ImGui::EndDisabled();
    }
    ImGui::End();

    if (raid.phase == RaidPhase::Active && !inventoryOpen)
    {
        const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        auto* draw = ImGui::GetForegroundDrawList();
        const ImU32 color = loot.Target() ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 255, 255, 220);
        draw->AddLine(ImVec2(center.x - 5, center.y), ImVec2(center.x + 5, center.y), color);
        draw->AddLine(ImVec2(center.x, center.y - 5), ImVec2(center.x, center.y + 5), color);
        ImGui::SetNextWindowPos(ImVec2(center.x, center.y + 28), ImGuiCond_Always, ImVec2(0.5f, 0));
        ImGui::Begin("Interaction", nullptr, overlay);
        if (const auto* item = loot.Target())
            ImGui::Text("[F] %s x%d", GetItemDefinition(item->stack.type).name, item->stack.quantity);
        else ImGui::TextUnformatted("Aim at a nearby item to pick it up");
        if (!loot.Message().empty()) ImGui::TextUnformatted(loot.Message().c_str());
        ImGui::End();
    }
    else if (raid.phase == RaidPhase::Active && inventoryOpen)
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x < 560 ? io.DisplaySize.x : 560,
            io.DisplaySize.y < 420 ? io.DisplaySize.y : 420));
        ImGui::Begin("Inventory [I to close]", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Slots %d/%d   Total weight %.2f kg", static_cast<int>(inventory.Items().size()),
            static_cast<int>(Inventory::Capacity), inventory.TotalWeight());
        ImGui::Separator();
        if (inventory.Items().empty()) ImGui::TextUnformatted("Empty. Find supplies and press F to pick them up.");
        if (ImGui::BeginTable("Items", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Item");
            ImGui::TableSetupColumn("Quantity");
            ImGui::TableSetupColumn("Weight (kg)");
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
            ImGui::Text("%s | %d/%d | Unit %.2f kg | Stack %.2f kg", definition.name, stack->quantity, definition.maxStack, definition.weight, definition.weight * stack->quantity);
            ImGui::TextUnformatted(stack->type == ItemType::Bandage ? "Effect: Health +20" : stack->type == ItemType::Water ? "Effect: Hydration +30" : "Ammunition: press R outside inventory to reload");
            ImGui::BeginDisabled(!Input::IsFocused() || !player.vitals.Alive());
            const bool usable = stack->type == ItemType::Bandage ? player.vitals.Health() < 100 : stack->type == ItemType::Water && player.vitals.Hydration() < 100;
            ImGui::BeginDisabled(!usable);
            if (ImGui::Button("Use")) actions.Request(InventoryAction::Use);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Drop 1")) actions.Request(InventoryAction::DropOne);
            ImGui::SameLine();
            if (ImGui::Button("Drop All")) actions.Request(InventoryAction::DropAll);
            ImGui::EndDisabled();
            if (!usable) ImGui::TextUnformatted("Cannot use: stat full or ammunition item.");
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
    void GearIcon(ImDrawList* draw, ImVec2 p, int type, ImU32 color)
    {
        if (type == 3) {
            draw->AddRectFilled({p.x+4,p.y+19},{p.x+56,p.y+27},color,2);
            draw->AddRectFilled({p.x+48,p.y+21},{p.x+72,p.y+24},color);
            draw->AddQuadFilled({p.x+5,p.y+21},{p.x+20,p.y+25},{p.x+15,p.y+35},{p.x,p.y+34},color);
            draw->AddRectFilled({p.x+30,p.y+27},{p.x+39,p.y+40},color,2);
        } else if (type == 0) {
            draw->AddRectFilled({p.x+12,p.y+6},{p.x+49,p.y+40},color,4);
            draw->AddRectFilled({p.x+27,p.y+12},{p.x+34,p.y+34},IM_COL32(80,33,29,255));
            draw->AddRectFilled({p.x+20,p.y+19},{p.x+41,p.y+26},IM_COL32(80,33,29,255));
        } else if (type == 1) {
            draw->AddRectFilled({p.x+24,p.y+3},{p.x+37,p.y+10},color,2);
            draw->AddRectFilled({p.x+19,p.y+11},{p.x+42,p.y+42},color,6);
            draw->AddRectFilled({p.x+20,p.y+22},{p.x+41,p.y+31},IM_COL32(40,62,73,255));
        } else {
            for (int i=0;i<4;++i) {
                float x=p.x+12+i*11;
                draw->AddRectFilled({x,p.y+15},{x+7,p.y+39},color,1);
                draw->AddTriangleFilled({x,p.y+15},{x+7,p.y+15},{x+3.5f,p.y+5},color);
            }
        }
    }
}

bool RaidHud::RenderHideout(Hideout& hideout)
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
    ImGui::Begin("Hideout", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(accent, "PROJECT TARKOV    /    HIDEOUT");
    ImGui::TextDisabled("CHARACTER & STASH                                      INDUSTRIAL ZONE / PREPARATION");
    ImGui::Separator();
    ImGui::TextWrapped("Drag equipment between stash and character. Drop supplies into the carried grid; drop a rifle into the weapon slot.");
    ImGui::BeginDisabled(!hideout.ready || !Input::IsFocused());

    GearDrag pending{};
    bool transfer = false, toStash = false;
    auto target = [&](bool stashTarget, int accepts) {
        if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload = ImGui::AcceptDragDropPayload(GearPayload)) {
                if (payload->DataSize == sizeof(GearDrag)) {
                    const auto data = *static_cast<const GearDrag*>(payload->Data);
                    if (data.fromStash != stashTarget && data.type >= 0 && data.type <= 3 &&
                        (accepts < 0 || (accepts == 3 ? data.type == 3 : data.type < 3))) {
                        pending = data; transfer = true; toStash = stashTarget;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    };
    auto card = [&](int id, const char* name, int type, int count, bool sourceStash, StackId stack, float width) {
        ImGui::PushID(id);
        const auto start = ImGui::GetCursorScreenPos();
        ImGui::Button("##gear", {width,96});
        auto* draw = ImGui::GetWindowDrawList();
        if (count > 0) GearIcon(draw,{start.x+8,start.y+6},type,
            type==1 ? IM_COL32(122,155,168,255) : IM_COL32(182,171,130,255));
        draw->AddText({start.x+8,start.y+57},IM_COL32(216,215,195,255),name);
        const std::string amount = count ? "x"+std::to_string(count) : "EMPTY";
        draw->AddText({start.x+8,start.y+76},IM_COL32(155,155,138,255),amount.c_str());
        if (count > 0 && ImGui::BeginDragDropSource()) {
            GearDrag data{type,sourceStash && type<3 ? (std::min)(count,GetItemDefinition(static_cast<ItemType>(type)).maxStack) : count,sourceStash,stack};
            ImGui::SetDragDropPayload(GearPayload,&data,sizeof(data));
            ImGui::Text("%s x%d",name,type==3?1:data.quantity);
            ImGui::EndDragDropSource();
        }
        target(sourceStash,sourceStash ? -1 : type==3?3:0);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(name);
            ImGui::TextUnformatted(sourceStash ? "Drag to character. One stack per transfer." : "Drag to stash to leave behind.");
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    };

    const bool wide=ImGui::GetContentRegionAvail().x>=850;
    const float panelHeight=(std::max)(440.f,io.DisplaySize.y-235.f);
    if (ImGui::BeginTable("HideoutLayout",wide?2:1,ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::BeginChild("CharacterPanel",{0,panelHeight},true);
        ImGui::TextColored(accent,"CHARACTER / LOADOUT");
        ImGui::TextDisabled("Equipment at risk during a raid");
        const auto p=ImGui::GetCursorScreenPos();
        const float center=p.x+ImGui::GetContentRegionAvail().x*.5f;
        auto* draw=ImGui::GetWindowDrawList();
        const ImU32 body=IM_COL32(69,76,65,255), edge=IM_COL32(123,130,104,255);
        draw->AddCircleFilled({center,p.y+24},17,body);
        draw->AddQuadFilled({center-27,p.y+47},{center+27,p.y+47},{center+21,p.y+117},{center-21,p.y+117},body);
        draw->AddLine({center-30,p.y+51},{center-45,p.y+112},edge,10);
        draw->AddLine({center+30,p.y+51},{center+45,p.y+112},edge,10);
        draw->AddLine({center-12,p.y+116},{center-20,p.y+172},edge,14);
        draw->AddLine({center+12,p.y+116},{center+20,p.y+172},edge,14);
        ImGui::Dummy({0,185});
        card(100,"PRIMARY / RIFLE",3,hideout.rifle?1:0,false,0,ImGui::GetContentRegionAvail().x);
        ImGui::Text("Magazine %d/30",hideout.magazine);
        ImGui::SameLine();
        ImGui::BeginDisabled(!hideout.rifle || hideout.magazine>=30 || hideout.stash[2]==0);
        if (ImGui::SmallButton("LOAD FROM STASH")) hideout.LoadMagazine();
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::Text("CARRIED SUPPLIES  %d/12 | %.2f kg",static_cast<int>(hideout.loadout.Items().size()),hideout.loadout.TotalWeight());
        const int columns=(std::max)(1,static_cast<int>(ImGui::GetContentRegionAvail().x/135));
        const float cell=(ImGui::GetContentRegionAvail().x-(columns-1)*8)/columns;
        const auto& items=hideout.loadout.Items();
        for (int i=0;i<static_cast<int>(Inventory::Capacity);++i) {
            if (i%columns) ImGui::SameLine();
            if (i<static_cast<int>(items.size())) {
                const auto& stack=items[i];
                card(200+i,GetItemDefinition(stack.type).name,static_cast<int>(stack.type),stack.quantity,false,stack.id,cell);
            } else card(200+i,"SUPPLY SLOT",0,0,false,0,cell);
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("StashPanel",{0,panelHeight},true);
        ImGui::TextColored(accent,"STASH / SECURE STORAGE");
        ImGui::TextWrapped("Stored items survive failed raids. Drop any carried item here to store it.");
        const float stashWidth=ImGui::GetContentRegionAvail().x;
        card(300,"RIFLE",3,hideout.rifles,true,0,stashWidth);
        for (int i=0;i<3;++i)
            card(301+i,GetItemDefinition(static_cast<ItemType>(i)).name,i,hideout.stash[i],true,0,stashWidth);
        ImGui::Button("DROP HERE TO STORE",{stashWidth,72});
        target(true,-1);
        ImGui::EndChild();
        ImGui::EndTable();
    }
    // Mutate only after every source item has been drawn, never while iterating inventory.
    if (transfer) {
        if (pending.type==3) {
            if (pending.fromStash ? !hideout.rifle && hideout.rifles>0 : hideout.rifle) hideout.ToggleRifle();
            else hideout.message="Weapon slot is occupied.";
        } else if (pending.quantity>0) {
            if (!toStash) hideout.Transfer(static_cast<ItemType>(pending.type),pending.quantity,true);
            else hideout.StoreStack(pending.stack);
        }
    }
    ImGui::Separator();
    ImGui::RadioButton("Southwest / NE exit",&hideout.spawn,0);
    ImGui::SameLine();
    ImGui::RadioButton("Northwest / SE exit",&hideout.spawn,1);
    if (!hideout.rifle) ImGui::TextColored(accent,"NO WEAPON EQUIPPED - entering unarmed");
    const bool deploy=ImGui::Button("ENTER RAID",{240,44});
    ImGui::SameLine();
    if (ImGui::Button("SAVE / RETRY",{140,44})) hideout.Save();
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",hideout.message.c_str());
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(8);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return deploy;
}
