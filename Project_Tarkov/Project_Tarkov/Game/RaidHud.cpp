#include <Windows.h>
#include "RaidHud.h"
#include "RaidStatus.h"
#include "../RaidSummary.h"
#include "Hideout.h"
#include "IndustrialZone.h"
#include <cmath>
#include <algorithm>
#include "Inventory.h"
#include "InventoryUi.h"
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

    // Closed Catmull-Rom contours keep shoulders, joints and extremities rounded.
    std::vector<ImVec2> CurvedContour(const std::vector<ImVec2>& control)
    {
        std::vector<ImVec2> curve;
        const int count=static_cast<int>(control.size());
        for (int i=0;i<count;++i) {
            const auto& a=control[(i+count-1)%count]; const auto& b=control[i];
            const auto& c=control[(i+1)%count]; const auto& d=control[(i+2)%count];
            for (int step=0;step<10;++step) {
                const float t=step/10.f,t2=t*t,t3=t2*t;
                auto interpolate=[&](float p0,float p1,float p2,float p3) {
                    return .5f*((2*p1)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t2+(-p0+3*p1-3*p2+p3)*t3);
                };
                curve.push_back({interpolate(a.x,b.x,c.x,d.x),interpolate(a.y,b.y,c.y,d.y)});
            }
        }
        return curve;
    }
    // One continuous anatomical outline per pose: head, neck, torso, hands and boots.
    // Front view standing; three-quarter side profiles for crouching and prone.
    void DrawStanceSilhouette(ImDrawList* draw,ImVec2 origin,bool crouching,bool prone,ImU32 color)
    {
        std::vector<ImVec2> contour;
        if (prone) contour={
            {6,104},{17,101},{30,99},{44,98},{60,94},{76,93},{87,90},
            {96,88},{101,79},{109,76},{119,78},{124,84},{125,92},{131,95},
            {126,98},{124,104},{115,107},{106,105},{101,109},{115,112},
            {129,111},{136,113},{137,118},{131,121},{115,122},{96,119},
            {87,114},{73,113},{57,116},{43,115},{30,117},{17,119},
            {8,124},{1,122},{0,116},{7,112},{0,111},{0,106}};
        else if (crouching) contour={
            {58,35},{68,34},{76,39},{79,48},{83,53},{78,57},{75,64},
            {66,66},{64,71},{72,76},{79,85},{86,91},{96,92},{101,96},
            {99,101},{91,103},{78,99},{70,91},{65,88},{64,98},
            {79,103},{86,111},{83,123},{78,131},{83,134},{94,135},
            {97,140},{92,143},{72,143},{68,138},{68,127},{72,116},
            {58,114},{47,111},{39,118},{33,130},{40,135},{43,140},
            {38,143},{19,143},{14,139},{18,130},{23,115},{31,101},
            {36,90},{40,78},{48,70},{53,67},{52,61},{48,54},{49,43}};
        else contour={
            {63,1},{72,3},{77,10},{78,20},{74,29},{69,34},{70,39},
            {81,43},{88,48},{92,59},{96,75},{98,88},{100,98},
            {98,104},{94,105},{90,99},{88,89},{84,77},{81,65},
            {77,67},{75,80},{79,93},{79,105},{75,121},{74,137},
            {78,143},{77,148},{61,149},{58,145},{60,136},{61,120},
            {60,108},{56,101},{52,110},{49,125},{48,139},{51,145},
            {48,149},{33,149},{31,145},{36,137},{37,120},{38,104},
            {38,94},{43,80},{42,68},{38,65},{34,80},{31,92},
            {30,101},{26,105},{22,102},{21,96},{24,86},{26,71},
            {29,57},{34,47},{44,42},{53,39},{54,34},{49,28},
            {46,19},{47,10},{53,3}};
        for (auto& point:contour) point={origin.x+point.x*.62f,origin.y+point.y*.62f};
        const auto curve=CurvedContour(contour);
        draw->AddConcavePolyFilled(curve.data(),static_cast<int>(curve.size()),color);
        draw->AddPolyline(curve.data(),static_cast<int>(curve.size()),IM_COL32(242,245,234,190),ImDrawFlags_Closed,.8f);
    }
    void DrawBodyMannequin(const PlayerVitals& vitals, ImVec2 origin, float scale, ImDrawList* target = nullptr)
    {
        auto* draw = target ? target : ImGui::GetWindowDrawList();

        auto polygon = [&](BodyPart part, std::initializer_list<ImVec2> vertices) {
            std::vector<ImVec2> points;
            for (auto p : vertices) points.push_back({origin.x+p.x*scale,origin.y+p.y*scale});
            points=CurvedContour(points);
            draw->AddConcavePolyFilled(points.data(),static_cast<int>(points.size()),BodyPartColor(vitals.BodyHealth(part)));
            draw->AddPolyline(points.data(),static_cast<int>(points.size()),IM_COL32(238,235,218,100),ImDrawFlags_Closed,scale);
        };
        polygon(BodyPart::Head,{{68,0},{79,4},{83,15},{79,28},{72,35},{64,35},{56,28},{53,15},{57,4}});
        polygon(BodyPart::Torso,{{61,30},{75,30},{77,45},{59,45}});
        polygon(BodyPart::Torso,{{40,46},{50,41},{60,39},{68,42},{77,39},{87,41},{96,47},
            {98,62},{93,79},{88,96},{87,111},{78,116},{58,116},{48,111},{47,96},{42,79},{37,62}});
        polygon(BodyPart::Pelvis,{{48,116},{59,121},{77,121},{88,116},{93,128},{98,142},
            {95,155},{86,162},{77,163},{68,156},{59,163},{50,162},{40,155},{38,142},{43,128}});
        polygon(BodyPart::LeftArm,{{30,46},{37,51},{37,64},{32,78},{28,93},{27,111},
            {24,126},{25,135},{21,144},{17,146},{13,142},{11,135},{14,122},{14,107},{15,91},{19,73},{22,56}});
        polygon(BodyPart::RightArm,{{106,46},{114,56},{117,73},{121,91},{122,107},{122,122},
            {125,135},{123,142},{119,146},{115,144},{111,135},{112,126},{109,111},{108,93},{104,78},{99,64},{99,51}});
        polygon(BodyPart::LeftLeg,{{44,162},{53,166},{65,164},{66,179},{62,195},{62,207},
            {60,222},{58,239},{61,246},{64,250},{62,255},{39,256},{37,251},{41,241},{40,225},{39,210},{40,197},{39,180}});
        polygon(BodyPart::RightLeg,{{74,164},{85,166},{94,162},{99,180},{98,197},{100,210},
            {98,225},{97,241},{102,251},{100,256},{77,255},{75,250},{78,246},{81,239},{79,222},{76,207},{76,195},{72,179}});

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

namespace
{
    struct CharacterPreview
    {
        CharacterVisual* visual;
        Character* character;
        Shader* shader;
        Camera camera;
        ImVec2 position;
        ImVec2 size;
        ImVec2 framebufferScale;
        bool rifle;
        bool updatePose = true;
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
        const int clipLeft=(std::max)(x,static_cast<int>(command->ClipRect.x*preview.framebufferScale.x));
        const int clipRight=(std::min)(x+width,static_cast<int>(command->ClipRect.z*preview.framebufferScale.x));
        const int clipBottom=(std::max)(y,viewport[1]+viewport[3]-static_cast<int>(command->ClipRect.w*preview.framebufferScale.y));
        const int clipTop=(std::min)(y+height,viewport[1]+viewport[3]-static_cast<int>(command->ClipRect.y*preview.framebufferScale.y));
        glScissor(clipLeft,clipBottom,(std::max)(0,clipRight-clipLeft),(std::max)(0,clipTop-clipBottom));
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        Renderer::PrepareFrame(*preview.shader,preview.camera,preview.size.x,preview.size.y);
        const float dt=std::clamp(std::isfinite(ImGui::GetIO().DeltaTime)?
            ImGui::GetIO().DeltaTime:0.0f,0.0f,0.1f);
        if (preview.updatePose) {
            preview.visual->SetLootPreview(preview.character->IsLooting());
            preview.visual->Update(dt,*preview.character,{false,false,false,false,0,0,preview.rifle});
        }
        preview.visual->Render(*preview.character,*preview.shader,preview.camera,preview.size.x,preview.size.y);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    }

}

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
    if (!itemImages.Load("Assets/UI/items.png")) throw std::runtime_error("Missing inventory image atlas");
    ImGui::StyleColorsDark();
    windowReady = ImGui_ImplGlfw_InitForOpenGL(window, true);
    if (!windowReady) throw std::runtime_error("HUD window initialization failed");
    rendererReady = ImGui_ImplOpenGL3_Init("#version 330");
    if (!rendererReady) throw std::runtime_error("HUD renderer initialization failed");
}

void RaidHud::Render(Player& player, LootSystem& loot, CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid,
    const CollisionWorld& world, Shader& shader, Enemy* corpse, int corpseTarget, StackId* takeStack, bool* takeWeapon, bool* closeLoot)
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
    if (raid.phase == RaidPhase::Active && (inventoryOpen || corpse)) {
        InventoryUi::Command command;
        ImGui::SetNextWindowPos({8,70},ImGuiCond_Always);
        ImGui::SetNextWindowSize({io.DisplaySize.x-16,(std::max)(200.f,io.DisplaySize.y-140)},ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.04f,.05f,.045f,corpse?.24f:.45f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.04f,.05f,.045f,corpse?.12f:.20f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg,ImVec4(.10f,.12f,.10f,.65f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive,ImVec4(.15f,.17f,.13f,.65f));
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.17f,.18f,.15f,.65f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.30f,.31f,.23f,.8f));
        ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.28f,.29f,.24f,.8f));
        InventoryUi::PanelOpacity=corpse?.65f:1.f;
        ImGui::Begin("인벤토리 · I / ESC 닫기",nullptr,ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::BeginDisabled(!Input::IsFocused() || !player.vitals.Alive());
        const float width=(ImGui::GetContentRegionAvail().x-16)/3;
        const float height=ImGui::GetContentRegionAvail().y-36;
        const auto texture=(ImTextureID)(intptr_t)itemImages.id;
        const float panelHeight=height;
        constexpr int RifleGear=ItemTypeCount;
        const Inventory* panelInventory=&inventory;
        std::uint32_t panelClothing=player.clothingMask;
        bool enemyPanel=false;
        CharacterPreview* activePreview=&inventoryPreview;
        ItemStack enemyRifle{ItemType::Rifle,1,1,30,true,-1};
    auto equipmentCard=[&](int id,const char* label,int type,ImVec2 p,ImVec2 size) {
        ImGui::PushID(id);
        const float header=ImGui::GetFontSize()+4.f;
        const ImVec2 bottom(p.x+size.x,p.y+size.y),body(p.x,p.y+header);
        const bool clothing=type>=0 && type<ItemTypeCount && IsClothing(static_cast<ItemType>(type));
        const bool weapon=type>=RifleGear && type<=RifleGear+2;
        const int weaponSlot=type==RifleGear ? -1 : type==RifleGear+1 ? -2 : -3;
        const int clothingSlot=clothing ? ClothingSlot(static_cast<ItemType>(type)) : 0;
        ItemStack worn{clothing?static_cast<ItemType>(type):ItemType::Bandage,1,static_cast<StackId>(clothingSlot+1)};
        const ItemStack* item=weapon?panelInventory->At(weaponSlot):
            clothing && (panelClothing&ClothingBit(worn.type))?&worn:nullptr;
        if (enemyPanel) {
            item=nullptr;
            if (clothing && (panelClothing&ClothingBit(worn.type)))
                for (const auto& entry:corpse->wornEquipment.Items())
                    if (entry.type==worn.type) { item=&entry; break; }
            if (type==RifleGear && corpse->rifle) item=&enemyRifle;
        }
        auto* draw=ImGui::GetWindowDrawList();
        ImGui::SetCursorScreenPos(body);
        if (enemyPanel && item) {
            InventoryUi::Cell(item,type==RifleGear?4:5,item->slot,texture,command,{size.x,size.y-header},.55f);
        } else if (!enemyPanel && (clothing || weapon)) {
            InventoryUi::Cell(item,clothing?2:0,clothing?clothingSlot:weaponSlot,texture,command,{size.x,size.y-header},.35f);
        } else {
            ImGui::InvisibleButton("EmptyEquipment",{size.x,size.y-header});
            draw->AddRectFilled(body,bottom,IM_COL32(34,38,38,35));
        }
        if (!item) {
            draw->PushClipRect(body,bottom,true);
            for (float x=p.x-size.y;x<bottom.x;x+=12.f)
                draw->AddLine({x,body.y},{x+size.y,bottom.y},IM_COL32(112,116,113,26),4.f);
            draw->PopClipRect();
        }
        draw->AddRectFilledMultiColor(p,{bottom.x,body.y},IM_COL32(104,109,108,90),
            IM_COL32(74,80,80,90),IM_COL32(48,53,54,90),IM_COL32(67,74,75,90));
        draw->AddRect(body,bottom,item?IM_COL32(186,157,108,255):IM_COL32(120,127,124,210));
        draw->PushClipRect(p,bottom,true);
        draw->AddText({p.x+4,p.y+1},IM_COL32(222,225,222,255),label);
        draw->AddText({bottom.x-14,p.y},IM_COL32(222,225,222,255),">");
        draw->PopClipRect();
        ImGui::SetCursorScreenPos({p.x,bottom.y+4});
        ImGui::PopID();
    };
    auto drawCarriedSlots=[&]() {
        const float width=ImGui::GetContentRegionAvail().x;
        const float gap=3.f,cell=(width-gap*5)/6;
        const bool vest=(panelClothing&ClothingBit(ItemType::Vest))!=0;
        const bool backpack=(panelClothing&ClothingBit(ItemType::Backpack))!=0;
        auto grid=[&](const char* title,int first,int count,int columns,ItemType gear) {
            ImGui::TextUnformatted(title);
            const auto origin=ImGui::GetCursorScreenPos();
            const bool hasGear=gear!=ItemType::Count;
            const float offset=hasGear?2*(cell+gap):0.f;
            if (hasGear) equipmentCard(650+static_cast<int>(gear),title,static_cast<int>(gear),origin,{cell*2+gap,cell*2+gap});
            InventoryUi::GridAt(*panelInventory,enemyPanel?1:0,first,count,columns,texture,command,
                {origin.x+offset,origin.y},cell,gap);
            const float rows=static_cast<float>((count+columns-1)/columns);
            ImGui::SetCursorScreenPos({origin.x,origin.y+(std::max)(hasGear?2.f:1.f,rows)*(cell+gap)+18});
        };
        grid("전술 조끼",12,vest?6:0,3,ItemType::Vest);
        grid("주머니",0,12,6,ItemType::Count);
        ImGui::TextUnformatted("특수 슬롯");
        const auto special=ImGui::GetCursorScreenPos();
        for (int i=0;i<3;++i) equipmentCard(680+i,"",-1,{special.x+i*(cell+gap),special.y},{cell,cell});
        ImGui::SetCursorScreenPos({special.x,special.y+cell+18});
        grid("배낭",18,backpack?12:0,4,ItemType::Backpack);
        ImGui::Separator();
        ImGui::Text("휴대품 %d / %d",panelInventory->StorageUsed(),panelInventory->SlotCapacity());
    };
    auto drawCharacterPanel=[&](const char* childId) {
        float& panelRotation=enemyPanel?enemyPreviewRotation:inventoryPreviewRotation;
        ImGui::BeginChild(childId,{width,panelHeight},true);
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const float width=ImGui::GetContentRegionAvail().x;
        const float scale=width/500.f;
        const float height=660.f*scale;
        auto* draw=ImGui::GetWindowDrawList();
        activePreview->position=origin;
        activePreview->size={width,height};
        draw->AddRectFilledMultiColor(origin,{origin.x+width,origin.y+height},
            IM_COL32(39,47,54,35),IM_COL32(55,54,42,35),IM_COL32(12,22,20,30),IM_COL32(14,23,23,30));
        draw->AddCallback(DrawCharacterPreview,activePreview);
        draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        auto slot=[&](int id,const char* name,int type,float x,float y,float w,float h) {
            equipmentCard(id,name,type,{origin.x+x*scale,origin.y+y*scale},{w*scale,h*scale});
        };
        slot(600,"헤드셋",-1,0,0,130,148);
        slot(601,"모자",static_cast<int>(ItemType::Helmet),185,0,130,148);
        slot(602,"얼굴",-1,370,0,130,148);
        slot(603,"완장",-1,0,168,130,66);
        slot(604,"인식표",-1,0,248,130,66);
        slot(605,"방탄복",static_cast<int>(ItemType::Vest),185,168,130,146);
        slot(606,"안경",-1,370,168,130,146);
        slot(607,"주무기 1",RifleGear,0,336,315,148);
        slot(608,"권총집",RifleGear+2,370,336,130,148);
        slot(609,"주무기 2",RifleGear+1,0,506,315,148);
        slot(610,"칼집",-1,370,506,130,148);
        ImGui::SetCursorScreenPos({origin.x+135*scale,origin.y+40*scale});
        if (ImGui::InvisibleButton("RotateCharacter",{45*scale,260*scale}))
            panelRotation=std::fmod(panelRotation+90.f,360.f);
        activePreview->character->rotation.y=panelRotation;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("클릭하여 캐릭터 회전");
        ImGui::SetCursorScreenPos({origin.x,origin.y+height+12});
        ImGui::Text("%s · %.1f kg",enemyPanel?"적 장비":"내 장비",panelInventory->TotalWeight());
        if (ImGui::CollapsingHeader("의상",enemyPanel?ImGuiTreeNodeFlags_DefaultOpen:0)) {
            const ItemType types[]{ItemType::Shirt,ItemType::Trousers,ItemType::UnderShirt,ItemType::Underpants,ItemType::Boots};
            for (int i=0;i<5;++i) {
                const ImVec2 p=ImGui::GetCursorScreenPos();
                const bool male = !enemyPanel && player.appearance == PlayerAppearance::Male;
                if (male && types[i] == ItemType::UnderShirt) continue;
                const auto type = ClothingType(ClothingSlot(types[i]),male);
                equipmentCard(620+i,GetItemDefinition(type).name,static_cast<int>(type),p,{width,76});
            }
        }
        if (enemyPanel) {
            if (corpse->rifle && ImGui::Button("소총 획득") && takeWeapon) *takeWeapon=true;
            if (ImGui::CollapsingHeader("적 휴대품",ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PushID("EnemyCarried");
                drawCarriedSlots();
                ImGui::PopID();
            }
            if (ImGui::Button("수색 종료") && closeLoot) *closeLoot=true;
        }
        ImGui::EndChild();
    };
    auto drawLoadoutPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{width,panelHeight},true);
        drawCarriedSlots();
        ImGui::TextWrapped("탄창 우클릭: 채우기 / 빼내기 · 총기에 끌어 삽입");
        ImGui::EndChild();
    };
        drawCharacterPanel("캐릭터 장비");
        ImGui::SameLine();
        drawLoadoutPanel("휴대 장비");
        ImGui::SameLine();
        if (corpse) {
            panelInventory=&corpse->inventory;
            panelClothing=corpse->body.clothingMask;
            enemyPanel=true;
            activePreview=&enemyPreview;
            drawCharacterPanel("적 캐릭터 장비");
        } else {
            ImGui::BeginChild("외부 장비",{width,height},true);
            ImGui::TextUnformatted("바닥 · 2.5m 이내 접근 가능한 물품");
            ImGui::Button("여기에 놓아 버리기",{-1,45});
            InventoryUi::Target(command,1,-100);
            const auto nearby=loot.Nearby(player,world);
            int index=0;
            for (const auto& item:nearby) {
                if (index%3) ImGui::SameLine();
                InventoryUi::Cell(&item,3,index++,texture,command,{width/3-12,width/3-12});
            }
            if (nearby.empty()) ImGui::TextDisabled("가까운 물품 없음");
            ImGui::EndChild();
        }
        ImGui::TextWrapped("%s",loot.Message().c_str());
        ImGui::EndDisabled(); ImGui::End();
        ImGui::PopStyleColor(7);
        InventoryUi::PanelOpacity=1.f;
        if (command.action!=InventoryUi::Action::None && Input::IsFocused() && player.vitals.Alive()) {
            bool ok=false;
            if (command.item.source==3 && command.action==InventoryUi::Action::Move && command.destination==0)
                ok=loot.TakeNearby(command.item.id,player,world,command.slot);
            else if (command.item.source==4 && command.destination==0 && corpse && corpse->rifle) {
                ItemStack weapon{ItemType::Rifle,1,0,30,true,command.slot};
                ok=command.slot==-100?inventory.Pickup(weapon):inventory.TryAdd(weapon);
                if (ok) corpse->rifle=false;
            } else if (!corpse && command.item.source==0 && command.action==InventoryUi::Action::Move && (command.destination==1 || command.destination==3)) {
                actions.selected=command.item.id; actions.Request(InventoryAction::DropAll); ok=true;
            } else if (command.action==InventoryUi::Action::Use && command.item.source==0 && inventory.Find(command.item.id) && inventory.Find(command.item.id)->type==ItemType::Bandage) {
                actions.selected=command.item.id; actions.Request(InventoryAction::Use); ok=true;
            } else if (command.item.source==5 && corpse && (command.destination==0 || command.destination==2)) {
                const auto* worn=corpse->wornEquipment.Find(command.item.id);
                if (worn) {
                    const auto remaining=corpse->body.clothingMask & ~ClothingBit(worn->type);
                    auto pockets=corpse->inventory;
                    command.item.source=1;
                    if (pockets.ConfigureGear(remaining) && InventoryUi::Execute(command,inventory,&corpse->wornEquipment,
                            player.clothingMask,player.vitals,player.appearance==PlayerAppearance::Male)) {
                        corpse->inventory=std::move(pockets);
                        corpse->body.clothingMask=remaining;
                        ok=true;
                    }
                }
            } else {
                ok=InventoryUi::Execute(command,inventory,corpse?&corpse->inventory:nullptr,player.clothingMask,player.vitals,player.appearance == PlayerAppearance::Male);
            }
            if (!ok) loot.Notify("이동/사용 불가: 공간, 장비 종류 또는 탄약을 확인하세요.");
        }
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

bool RaidHud::RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader, bool mapPanel)
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
    ImGui::BeginDisabled(!mapPanel && !hideout.vitals.Alive());
    const bool deploy=ImGui::Button(mapPanel ? "은신처로 돌아가기" : "레이드 출격",{150,34});
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("저장 / 재시도",{135,34})) hideout.Save();
    ImGui::SameLine();
    if (ImGui::Button("물 마시기",{110,34})) hideout.DrinkWater();
    ImGui::SameLine();
    ImGui::TextUnformatted(hideout.message.c_str());
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(!hideout.ready || !Input::IsFocused());

    InventoryUi::Command command;
    const auto texture=(ImTextureID)(intptr_t)itemImages.id;
    constexpr int RifleGear=ItemTypeCount;
    const bool wide=ImGui::GetContentRegionAvail().x>=1050;
    const float available=ImGui::GetContentRegionAvail().y;
    const float panelHeight=(std::max)(180.f,wide||hideoutHealthTab?available:(available-8.f)*.5f);
    Player previewCharacter;
    previewCharacter.position={0,0,0};
    previewCharacter.rotation.y=hideoutPreviewRotation;
    previewCharacter.clothingMask=hideout.clothingMask;
    CharacterPreview preview{&visual,&previewCharacter,&shader,{}, {}, {}, io.DisplayFramebufferScale,hideout.rifle};
    preview.camera.position={0,1.0f,3.4f};
    preview.camera.front={0,0,-1};
    preview.camera.up={0,1,0};
    preview.camera.fieldOfView=38.f;

    // Restore the original equipment-card chrome and positions around the model.
    auto equipmentCard=[&](int id,const char* label,int type,ImVec2 p,ImVec2 size) {
        ImGui::PushID(id);
        const float header=ImGui::GetFontSize()+4.f;
        const ImVec2 bottom(p.x+size.x,p.y+size.y),body(p.x,p.y+header);
        const bool clothing=type>=0 && type<ItemTypeCount && IsClothing(static_cast<ItemType>(type));
        const bool weapon=type>=RifleGear && type<=RifleGear+2;
        const int weaponSlot=type==RifleGear ? -1 : type==RifleGear+1 ? -2 : -3;
        const int clothingSlot=clothing ? ClothingSlot(static_cast<ItemType>(type)) : 0;
        ItemStack worn{clothing?static_cast<ItemType>(type):ItemType::Bandage,1,static_cast<StackId>(clothingSlot+1)};
        const ItemStack* item=weapon?hideout.loadout.At(weaponSlot):
            clothing && (hideout.clothingMask&ClothingBit(worn.type))?&worn:nullptr;
        auto* draw=ImGui::GetWindowDrawList();
        ImGui::SetCursorScreenPos(body);
        if (clothing || weapon) {
            InventoryUi::Cell(item,clothing?2:0,clothing?clothingSlot:weaponSlot,texture,command,{size.x,size.y-header},.35f);
        } else {
            ImGui::InvisibleButton("EmptyEquipment",{size.x,size.y-header});
            draw->AddRectFilled(body,bottom,IM_COL32(34,38,38,35));
        }
        if (!item) {
            draw->PushClipRect(body,bottom,true);
            for (float x=p.x-size.y;x<bottom.x;x+=12.f)
                draw->AddLine({x,body.y},{x+size.y,bottom.y},IM_COL32(112,116,113,26),4.f);
            draw->PopClipRect();
        }
        draw->AddRectFilledMultiColor(p,{bottom.x,body.y},IM_COL32(104,109,108,90),
            IM_COL32(74,80,80,90),IM_COL32(48,53,54,90),IM_COL32(67,74,75,90));
        draw->AddRect(body,bottom,item?IM_COL32(186,157,108,255):IM_COL32(120,127,124,210));
        draw->PushClipRect(p,bottom,true);
        draw->AddText({p.x+4,p.y+1},IM_COL32(222,225,222,255),label);
        draw->AddText({bottom.x-14,p.y},IM_COL32(222,225,222,255),">");
        draw->PopClipRect();
        ImGui::SetCursorScreenPos({p.x,bottom.y+4});
        ImGui::PopID();
    };
    auto drawCharacterPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        int selected = hideout.appearance == PlayerAppearance::Male ? 1 : 0;
        if (ImGui::Combo("캐릭터 변경", &selected, "여캐\0남캐\0")) {
            if (hideout.ChangeAppearance(selected == 1 ? PlayerAppearance::Male : PlayerAppearance::Female)) {
                character.appearance = hideout.appearance;
                visual.Init(character.appearance);
                inventoryPreviewVisual.reset();
                        }
        }
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const float width=ImGui::GetContentRegionAvail().x;
        const float scale=width/500.f;
        const float height=660.f*scale;
        auto* draw=ImGui::GetWindowDrawList();
        preview.position=origin;
        preview.size={width,height};
        draw->AddRectFilledMultiColor(origin,{origin.x+width,origin.y+height},
            IM_COL32(39,47,54,255),IM_COL32(55,54,42,255),IM_COL32(12,22,20,255),IM_COL32(14,23,23,255));
        draw->AddCallback(DrawCharacterPreview,&preview);
        draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        auto slot=[&](int id,const char* name,int type,float x,float y,float w,float h) {
            equipmentCard(id,name,type,{origin.x+x*scale,origin.y+y*scale},{w*scale,h*scale});
        };
        slot(600,"헤드셋",-1,0,0,130,148);
        slot(601,"모자",static_cast<int>(ItemType::Helmet),185,0,130,148);
        slot(602,"얼굴",-1,370,0,130,148);
        slot(603,"완장",-1,0,168,130,66);
        slot(604,"인식표",-1,0,248,130,66);
        slot(605,"방탄복",static_cast<int>(ItemType::Vest),185,168,130,146);
        slot(606,"안경",-1,370,168,130,146);
        slot(607,"주무기 1",RifleGear,0,336,315,148);
        slot(608,"권총집",RifleGear+2,370,336,130,148);
        slot(609,"주무기 2",RifleGear+1,0,506,315,148);
        slot(610,"칼집",-1,370,506,130,148);
        ImGui::SetCursorScreenPos({origin.x+135*scale,origin.y+40*scale});
        if (ImGui::InvisibleButton("RotateCharacter",{45*scale,260*scale}))
            hideoutPreviewRotation=std::fmod(hideoutPreviewRotation+90.f,360.f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("클릭하여 캐릭터 회전");
        ImGui::SetCursorScreenPos({origin.x,origin.y+height+12});
        ImGui::Text("무게 %.1f kg",hideout.loadout.TotalWeight());
        ImGui::SameLine();
        ImGui::TextColored({.72f,.86f,.30f,1},"체력 %.0f",hideout.vitals.Health());
        ImGui::TextColored({.39f,.71f,.85f,1},"수분 %.0f",hideout.vitals.Hydration());
        ImGui::SameLine();
        ImGui::Text("스태미나 %.0f",hideout.vitals.Stamina());
        if (ImGui::CollapsingHeader("의상")) {
            const ItemType types[]{ItemType::Shirt,ItemType::Trousers,ItemType::UnderShirt,ItemType::Underpants,ItemType::Boots};
            for (int i=0;i<5;++i) {
                const ImVec2 p=ImGui::GetCursorScreenPos();
                const bool male = hideout.appearance == PlayerAppearance::Male;
                if (male && types[i] == ItemType::UnderShirt) continue;
                const auto type = ClothingType(ClothingSlot(types[i]),male);
                equipmentCard(620+i,GetItemDefinition(type).name,static_cast<int>(type),p,{width,76});
            }
        }
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
            const auto& rect=rectangles[static_cast<std::size_t>(i)];
            ImGui::SetCursorScreenPos({rect.x,rect.y});
            ImGui::PushID(700+i);
            ImGui::InvisibleButton("HealDrop",{rect.z-rect.x,rect.w-rect.y});
            if (ImGui::IsItemClicked()) hideout.HealBodyPart(static_cast<BodyPart>(i));
            if (ImGui::BeginDragDropTarget()) {
                if (const auto* payload=ImGui::AcceptDragDropPayload("INVENTORY_OBJECT")) {
                    if (payload->DataSize==sizeof(InventoryUi::Drag)) {
                        const auto data=*static_cast<const InventoryUi::Drag*>(payload->Data);
                        const auto* item=data.source==0?hideout.loadout.Find(data.id):data.source==1?hideout.storage.Find(data.id):nullptr;
                        if (item && item->type==ItemType::Bandage)
                            command={InventoryUi::Action::Heal,data,0,i};
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %.0f / 100\n클릭하거나 붕대를 끌어 놓아 치료",BodyPartNames[i],hideout.vitals.BodyHealth(static_cast<BodyPart>(i)));
            ImGui::PopID();
        }
        const ImVec2 modelPosition(origin.x+panelWidth*.43f,origin.y+8.f);
        const ImVec2 modelSize(panelWidth*.55f,panelHeight-24.f);
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(modelPosition,{modelPosition.x+modelSize.x,modelPosition.y+modelSize.y},IM_COL32(25,29,26,235),3.f);
        preview.position=modelPosition; preview.size=modelSize;
        draw->AddCallback(DrawCharacterPreview,&preview);
        draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        ImGui::EndChild();
    };
    auto drawLoadoutPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        const float width=ImGui::GetContentRegionAvail().x;
        const float gap=3.f,cell=(width-gap*5)/6;
        const bool vest=(hideout.clothingMask&ClothingBit(ItemType::Vest))!=0;
        const bool backpack=(hideout.clothingMask&ClothingBit(ItemType::Backpack))!=0;
        auto grid=[&](const char* title,int first,int count,int columns,ItemType gear) {
            ImGui::TextUnformatted(title);
            const auto origin=ImGui::GetCursorScreenPos();
            const bool hasGear=gear!=ItemType::Count;
            const float offset=hasGear?2*(cell+gap):0.f;
            if (hasGear) equipmentCard(650+static_cast<int>(gear),title,static_cast<int>(gear),origin,{cell*2+gap,cell*2+gap});
            InventoryUi::GridAt(hideout.loadout,0,first,count,columns,texture,command,
                {origin.x+offset,origin.y},cell,gap);
            const float rows=static_cast<float>((count+columns-1)/columns);
            ImGui::SetCursorScreenPos({origin.x,origin.y+(std::max)(hasGear?2.f:1.f,rows)*(cell+gap)+18});
        };
        grid("전술 조끼",12,vest?6:0,3,ItemType::Vest);
        grid("주머니",0,12,6,ItemType::Count);
        ImGui::TextUnformatted("특수 슬롯");
        const auto special=ImGui::GetCursorScreenPos();
        for (int i=0;i<3;++i) equipmentCard(680+i,"",-1,{special.x+i*(cell+gap),special.y},{cell,cell});
        ImGui::SetCursorScreenPos({special.x,special.y+cell+18});
        grid("배낭",18,backpack?12:0,4,ItemType::Backpack);
        ImGui::Separator();
        ImGui::Text("휴대품 %d / %d",hideout.loadout.StorageUsed(),hideout.loadout.SlotCapacity());
        ImGui::TextWrapped("탄창 우클릭: 채우기 / 빼내기 · 총기에 끌어 삽입");
        ImGui::EndChild();
    };
    auto drawStashPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        ImGui::SetNextItemWidth((std::max)(80.f,ImGui::GetContentRegionAvail().x-115.f));
        ImGui::InputTextWithHint("##StashSearch","검색",hideoutStashSearch,sizeof(hideoutStashSearch));
        ImGui::SameLine();
        if (ImGui::SmallButton("자동정리")) hideout.AutoArrangeStash();
        const auto available=ImGui::GetContentRegionAvail();
        ImGui::BeginChild("StashGridScroll",{available.x,(std::max)(64.f,available.y)},true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar|ImGuiWindowFlags_HorizontalScrollbar);
        constexpr int columns=Hideout::StashColumns;
        const float gap=2.f;
        const float cell=(std::max)(32.f,(std::min)((ImGui::GetContentRegionAvail().x-(columns-1)*gap)/columns,80.f));
        const auto origin=ImGui::GetCursorScreenPos();
        InventoryUi::GridAt(hideout.storage,1,0,hideout.storage.SlotCapacity(),columns,
            texture,command,origin,cell,gap,hideoutStashSearch);
        ImGui::EndChild(); ImGui::EndChild();
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
            ImGui::TableSetupColumn("캐릭터",ImGuiTableColumnFlags_WidthStretch,1.15f);
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
    ImGui::EndDisabled();
    if (command.action!=InventoryUi::Action::None && hideout.ready && Input::IsFocused()) {
        auto before=hideout;
        if (!InventoryUi::Execute(command,hideout.loadout,&hideout.storage,hideout.clothingMask,hideout.vitals,hideout.appearance == PlayerAppearance::Male))
            hideout.message="이동/사용 불가: 빈 공간, 장비 종류, 탄약을 확인하세요.";
        else if (!hideout.Save()) { const auto failure=hideout.message; hideout=std::move(before); hideout.message=failure; }
    }
    ImGui::End();
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(8);
    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
