#pragma once
#include "../Character/PlayerVitals.h"
#include "../../Graphics/Camera.h"
#include <imgui.h>
#include <array>
class CharacterVisual;
class Character;
class Shader;
namespace HudWidgets {
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

std::array<ImVec4, BodyPartCount> BodyPartRects(ImVec2 origin, float scale);
void DrawStanceSilhouette(ImDrawList*, ImVec2, bool, bool, ImU32);
void DrawBodyMannequin(const PlayerVitals&, ImVec2, float, ImDrawList* target = nullptr);
void DrawEquipmentCardFrame(ImVec2, ImVec2, const char*, bool);
void DrawCharacterPreview(const ImDrawList*, const ImDrawCmd*);
void DrawHideoutExitMenu(bool&, ImVec2);
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
