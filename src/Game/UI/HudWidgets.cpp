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

namespace HudWidgets
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
    void DrawBodyMannequin(const PlayerVitals& vitals, ImVec2 origin, float scale, ImDrawList* target)
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


}

namespace HudWidgets
{

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

namespace HudWidgets
{
    void DrawHideoutExitMenu(bool& open, ImVec2 displaySize)
    {
        if (!open) return;
        ImGui::SetNextWindowPos({0,0},ImGuiCond_Always);
        ImGui::SetNextWindowSize(displaySize,ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::Begin("HideoutExitOverlay",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled({0,0},displaySize,IM_COL32(0,0,0,170));
        const ImVec2 panelSize(360,172);
        const ImVec2 panelOrigin((displaySize.x-panelSize.x)*.5f,(displaySize.y-panelSize.y)*.5f);
        draw->AddRectFilled(panelOrigin,{panelOrigin.x+panelSize.x,panelOrigin.y+panelSize.y},
            IM_COL32(28,32,31,248),4.f);
        ImGui::SetCursorScreenPos({panelOrigin.x+24,panelOrigin.y+24});
        ImGui::TextUnformatted("게임 일시 정지");
        ImGui::SetCursorScreenPos({panelOrigin.x+24,panelOrigin.y+88});
        if (ImGui::Button("게임으로 돌아가기",{150,42})) open=false;
        ImGui::SameLine();
        if (ImGui::Button("게임 종료",{150,42}))
            glfwSetWindowShouldClose(glfwGetCurrentContext(),GLFW_TRUE);
        ImGui::End();
        ImGui::PopStyleVar();
    }
}


namespace HudWidgets {
void DrawEquipmentCardFrame(ImVec2 p,ImVec2 size,const char* label,bool item) {
    auto* draw=ImGui::GetWindowDrawList();
    const float header=ImGui::GetFontSize()+4.f;
    const ImVec2 bottom(p.x+size.x,p.y+size.y),body(p.x,p.y+header);
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
}
}
