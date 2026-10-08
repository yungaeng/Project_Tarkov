#include "RaidHud.h"
#include "Inventory.h"
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

void RaidHud::Render(const Inventory& inventory, const LootSystem& loot, bool inventoryOpen)
{
    if (!rendererReady) return;
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
    ImGui::End();

    if (!inventoryOpen)
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
    else
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
                ImGui::TableNextColumn(); ImGui::TextUnformatted(item.name);
                ImGui::TableNextColumn(); ImGui::Text("%d / %d", stack.quantity, item.maxStack);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", stack.quantity * item.weight);
            }
            ImGui::EndTable();
        }
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
