#include "../Inventory/RaidInventoryCommands.h"
#include "../../Core/AssetCatalog.h"
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

void RaidHud::RenderInventoryPanel(Player& player, LootSystem& loot, InventoryActions& actions,
    const CollisionWorld& world, Enemy* corpse, HudWidgets::CharacterPreview& inventoryPreview,
    HudWidgets::CharacterPreview& enemyPreview, bool* takeWeapon, bool* closeLoot)
{
    auto& inventory=player.GetInventory();
    const auto& io=ImGui::GetIO();

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
        DrawEquipmentCardFrame(p,size,label,item!=nullptr);
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
            InventoryCommands::ApplyRaid(command,player,loot,world,actions,corpse);
        }
    }
