#include "RaidHud.h"
#include "RaidStatus.h"
#include "Hideout.h"
#include "IndustrialZone.h"
#include <cmath>
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

bool RaidHud::RenderHideout(Hideout& hideout)
{
    if (!rendererReady) return false;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Hideout", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted("HIDEOUT / Industrial Zone");
    ImGui::TextWrapped("Prepare your loadout. Stored equipment stays safe. Carried equipment is lost on death, MIA or quitting a raid.");
    ImGui::Separator();
    ImGui::BeginDisabled(!hideout.ready || !Input::IsFocused());
    ImGui::Text("Weapon slot: %s | Stashed rifles: %d", hideout.rifle ? "Rifle equipped" : "Empty", hideout.rifles);
    if (ImGui::Button(hideout.rifle ? "Store rifle and unload" : "Equip rifle")) hideout.ToggleRifle();
    ImGui::SameLine();
    ImGui::BeginDisabled(!hideout.rifle || hideout.magazine >= 30 || hideout.stash[2] == 0);
    if (ImGui::Button("Load magazine from stash")) hideout.LoadMagazine();
    ImGui::EndDisabled();
    ImGui::Text("Magazine: %d/30 | Carried supplies: %d/%d slots | %.2f kg", hideout.magazine,
        static_cast<int>(hideout.loadout.Items().size()), static_cast<int>(Inventory::Capacity), hideout.loadout.TotalWeight());
    if (ImGui::BeginTable("Equipment", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        for (const char* heading : {"Item", "Stash", "Carried", "Take", "Leave behind"}) ImGui::TableSetupColumn(heading);
        ImGui::TableHeadersRow();
        for (int i = 0; i < 3; ++i) {
            const auto type = static_cast<ItemType>(i);
            const int step = type == ItemType::Ammo ? 30 : 1;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(GetItemDefinition(type).name);
            ImGui::TableNextColumn(); ImGui::Text("%d", hideout.stash[i]);
            ImGui::TableNextColumn(); ImGui::Text("%d", hideout.loadout.Count(type));
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(hideout.stash[i] == 0);
            if (ImGui::Button(type == ItemType::Ammo ? "Take 30" : "Take 1")) hideout.Transfer(type, step, true);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(hideout.loadout.Count(type) == 0);
            if (ImGui::Button(type == ItemType::Ammo ? "Store 30" : "Store 1")) hideout.Transfer(type, step, false);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Deployment");
    ImGui::RadioButton("Southwest spawn / NE exit", &hideout.spawn, 0);
    ImGui::RadioButton("Northwest spawn / SE exit", &hideout.spawn, 1);
    if (!hideout.rifle) ImGui::TextUnformatted("No weapon selected: you will enter unarmed.");
    const bool deploy = ImGui::Button("ENTER RAID", ImVec2(240, 48));
    ImGui::SameLine();
    if (ImGui::Button("Save / Retry")) hideout.Save();
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s", hideout.message.c_str());
    ImGui::TextUnformatted("Q: Quit | Changes are saved automatically.");
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return deploy;
}
