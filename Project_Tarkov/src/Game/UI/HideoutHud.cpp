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

void RaidHud::RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader,
    bool mapPanel, bool* exitMenuOpen, int storageRoom, int storageFurniture)
{
    if (!rendererReady) return;
    const bool furnitureMode=storageRoom>=0 && storageRoom<Hideout::HideoutRoomCount &&
        storageFurniture>=0 && storageFurniture<Hideout::FurnitureCount;
    const auto furnitureKind=furnitureMode?static_cast<HideoutFurniture>(storageFurniture):HideoutFurniture::Refrigerator;
    const auto furnitureFilter=furnitureMode?Hideout::FilterFor(furnitureKind):StorageFilter::None;
    const int furnitureSource=furnitureMode?Hideout::FurnitureStorageSource(storageRoom,furnitureKind):-1;
    Inventory& shownStorage=furnitureMode
        ? hideout.FurnitureStorageAt(storageRoom,furnitureKind) : hideout.storage;
    if (furnitureMode) hideoutHealthTab=false;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    const ImVec4 accent(.66f,.61f,.43f,1);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.045f,.05f,.047f,.58f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.07f,.077f,.07f,.58f));
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
    ImGui::SameLine();
    ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(),io.DisplaySize.x*.55f));
    ImGui::TextDisabled(furnitureMode ? "F 보관함 닫기 | Esc 메뉴" :
        mapPanel ? "F 은신처로 돌아가기 | Esc 메뉴" : "Esc 메뉴");
    const auto balance=std::string("보유 루블: ")+std::to_string(hideout.rubles)+" RUB";
    const float balanceWidth=ImGui::CalcTextSize(balance.c_str()).x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(io.DisplaySize.x-20.f-balanceWidth);
    ImGui::TextColored(accent,"%s",balance.c_str());
    ImGui::Separator();
    if (!hideout.message.empty()) ImGui::TextDisabled("%s",hideout.message.c_str());
    const bool uiEnabled=hideout.ready && Input::IsFocused() && (!exitMenuOpen || !*exitMenuOpen);
    ImGui::BeginDisabled(!uiEnabled);

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
        DrawEquipmentCardFrame(p,size,label,item!=nullptr);
        ImGui::SetCursorScreenPos({p.x,bottom.y+4});
        ImGui::PopID();
    };
    auto drawCharacterPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const float width=ImGui::GetContentRegionAvail().x;
        const float scale=width/500.f;
        const float height=660.f*scale;
        auto* draw=ImGui::GetWindowDrawList();
        preview.position=origin;
        preview.size={width,height};
        draw->AddRectFilledMultiColor(origin,{origin.x+width,origin.y+height},
            IM_COL32(39,47,54,150),IM_COL32(55,54,42,150),IM_COL32(12,22,20,150),IM_COL32(14,23,23,150));
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
        if (ImGui::SmallButton("물 마시기")) hideout.DrinkWater();
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
        draw->AddRectFilled(modelPosition,{modelPosition.x+modelSize.x,modelPosition.y+modelSize.y},IM_COL32(25,29,26,170),3.f);
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
                {origin.x+offset,origin.y},cell,gap,"",false,false,
                furnitureFilter,furnitureSource);
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
        const char* storageNames[]{"냉장고","무기고","옷장","의료품 탁자"};
        const char* storageItemNames[]{"음식·물","무기·탄약","의류","의료품"};
        if (furnitureMode)
            ImGui::Text("%s · %s",storageNames[storageFurniture],storageItemNames[storageFurniture]);
        ImGui::SetNextItemWidth((std::max)(80.f,ImGui::GetContentRegionAvail().x*.5f));
        ImGui::InputTextWithHint("##StashSearch","검색",hideoutStashSearch,sizeof(hideoutStashSearch));
        ImGui::SameLine();
        if (ImGui::SmallButton("자동정리"))
            hideout.AutoArrangeStash(furnitureMode ? storageRoom : -1,
                furnitureMode ? storageFurniture : -1);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("현재 보관함 전체를 정리합니다. 검색으로 숨겨진 아이템도 포함됩니다.");
        StorageFilter displayFilter=furnitureFilter;
        if (!furnitureMode) {
            const char* categories[]{"전체","무기","탄약·탄창","의류","의료품","음식·물","기타"};
            const StorageFilter filters[]{StorageFilter::None,StorageFilter::Weapons,
                StorageFilter::Ammunition,StorageFilter::Clothing,StorageFilter::Medical,
                StorageFilter::Consumables,StorageFilter::Other};
            if (ImGui::BeginTabBar("StashCategories",ImGuiTabBarFlags_FittingPolicyScroll)) {
                for (int i=0;i<7;++i) {
                    if (ImGui::BeginTabItem(categories[i])) {
                        hideoutStashCategory=i;
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
            displayFilter=filters[hideoutStashCategory];
        }
        const auto available=ImGui::GetContentRegionAvail();
        ImGui::BeginChild("StashGridScroll",{available.x,(std::max)(64.f,available.y)},true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar|ImGuiWindowFlags_HorizontalScrollbar);
        constexpr int columns=Hideout::StashColumns;
        const float gap=2.f;
        const float cell=(std::max)(32.f,(std::min)((ImGui::GetContentRegionAvail().x-(columns-1)*gap)/columns,80.f));
        const auto origin=ImGui::GetCursorScreenPos();
        InventoryUi::GridAt(shownStorage,furnitureMode?furnitureSource:1,0,shownStorage.SlotCapacity(),columns,
            texture,command,origin,cell,gap,hideoutStashSearch,true,
            hideout.appearance == PlayerAppearance::Male,furnitureFilter,
            furnitureSource,displayFilter);
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
    if (uiEnabled) hideout.ApplyInventoryCommand(command,storageRoom,storageFurniture);
    if (exitMenuOpen && *exitMenuOpen)
        DrawHideoutExitMenu(*exitMenuOpen,io.DisplaySize);
    ImGui::End();
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(8);
    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RaidHud::RenderHideoutStorage(Hideout& hideout, int room, int furniture, Player& character,
    CharacterVisual& visual, Shader& shader, bool& panelOpen, bool& exitMenuOpen)
{
    if (!panelOpen || room<0 || room>=Hideout::HideoutRoomCount ||
        furniture<0 || furniture>=Hideout::FurnitureCount) return;
    RenderHideout(hideout,character,visual,shader,true,&exitMenuOpen,room,furniture);
}

void RaidHud::RenderHideoutPauseMenu(bool& exitMenuOpen)
{
    if (!rendererReady || !exitMenuOpen) return;
    DrawHideoutExitMenu(exitMenuOpen,ImGui::GetIO().DisplaySize);
}
