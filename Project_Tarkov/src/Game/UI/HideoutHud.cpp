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
    bool mapPanel, bool* exitMenuOpen, int storageRoom, int storageFurniture, const CollisionWorld* world)
{
    if (!rendererReady) return;
    const bool furnitureMode=storageRoom>=0 && storageRoom<Hideout::HideoutRoomCount &&
        storageFurniture>=0 && storageFurniture<Hideout::FurnitureCount;
    const bool caseOpen=storageRoom==0 && storageFurniture==Hideout::PelicanStorage;
    const bool storageOpen=furnitureMode || caseOpen;
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
    ImGui::TextDisabled(storageOpen ? "Tab / F 보관함 닫기 | Esc 메뉴" :
        mapPanel ? "Tab / F 닫기 | Esc 메뉴" : "Tab 인벤토리 | Esc 메뉴");
    const auto balance=std::string("보유 루블: ")+std::to_string(hideout.rubles)+" RUB";
    const float balanceWidth=ImGui::CalcTextSize(balance.c_str()).x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(io.DisplaySize.x-20.f-balanceWidth);
    ImGui::TextColored(accent,"%s",balance.c_str());
    ImGui::Separator();
    if (!hideout.message.empty()) ImGui::TextDisabled("%s",hideout.message.c_str());
    if (!storageOpen) ImGui::TextDisabled("창고는 1번 방 펠리컨 케이스 앞에서 F로 열 수 있습니다.");
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
            InventoryUi::Cell(item,clothing?2:0,clothing?clothingSlot:weaponSlot,texture,command,{size.x,size.y-header},.35f,
                false,false,StorageFilter::None,-1,caseOpen?1:3);
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
        if (ImGui::SmallButton("물 마시기")) hideout.DrinkWater(0,false);
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
            if (ImGui::IsItemClicked()) hideout.HealBodyPart(static_cast<BodyPart>(i),caseOpen);
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
    auto drawFloorPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        ImGui::TextUnformatted("바닥 · 2.5m 이내 접근 가능한 물품");
        ImGui::Button("여기에 놓아 내려놓기",{-1,45});
        InventoryUi::Target(command,3,-100);
        const auto nearby=world ? hideout.floorLoot.Nearby(character,*world) : std::vector<ItemStack>{};
        const float cell=(std::max)(32.f,(ImGui::GetContentRegionAvail().x-16.f)/3.f);
        int index=0;
        for (const auto& item:nearby) {
            if (index%3) ImGui::SameLine();
            InventoryUi::Cell(&item,3,index++,texture,command,{cell,cell});
        }
        if (nearby.empty()) ImGui::TextDisabled("가까운 물품 없음");
        ImGui::EndChild();
    };
    auto drawStashPanel=[&](const char* childId) {
        ImGui::BeginChild(childId,{0,panelHeight},true);
        const char* storageNames[]{"냉장고","무기고","옷장","의료품 탁자"};
        const char* storageItemNames[]{"음식·물","무기·탄약","의류","의료품"};
        if (furnitureMode)
            ImGui::Text("%s · %s",storageNames[storageFurniture],storageItemNames[storageFurniture]);
        else ImGui::TextUnformatted("펠리컨 케이스 · 창고");
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
            if (storageOpen) drawStashPanel("HealthStashPanel");
            else drawFloorPanel("HealthFloorPanel");
            ImGui::EndTable();
        }
    } else if (ImGui::BeginTable(wide?"HideoutLayout":"HideoutLayoutNarrow",wide?3:2,
        ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)) {
        if (wide) {
            ImGui::TableSetupColumn("캐릭터",ImGuiTableColumnFlags_WidthStretch,1.15f);
            ImGui::TableSetupColumn("인벤토리",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableSetupColumn(storageOpen?"창고":"바닥",ImGuiTableColumnFlags_WidthStretch,1.45f);
            ImGui::TableNextColumn();
            drawCharacterPanel("CharacterPanel");
            ImGui::TableNextColumn();
            drawLoadoutPanel("LoadoutPanel");
            ImGui::TableNextColumn();
            if (storageOpen) drawStashPanel("StashPanel");
            else drawFloorPanel("FloorPanel");
        } else {
            ImGui::TableSetupColumn("캐릭터와 인벤토리",ImGuiTableColumnFlags_WidthStretch,1.1f);
            ImGui::TableSetupColumn(storageOpen?"창고":"바닥",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableNextColumn();
            drawCharacterPanel("CharacterPanelNarrow");
            drawLoadoutPanel("LoadoutPanelNarrow");
            ImGui::TableNextColumn();
            if (storageOpen) drawStashPanel("StashPanelNarrow");
            else drawFloorPanel("FloorPanelNarrow");
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if (uiEnabled) {
        const bool floorDrop=command.item.source!=3 && command.action==InventoryUi::Action::Move &&
            (command.destination==3 || (!storageOpen && command.destination==1));
        if (command.item.source==3 && command.action!=InventoryUi::Action::None && world)
            hideout.TakeFromFloor(command,character,*world);
        else if (floorDrop && world) hideout.DropToFloor(command,character.position,*world);
        else if (!floorDrop) hideout.ApplyInventoryCommand(command,
            furnitureMode?storageRoom:-1,furnitureMode?storageFurniture:-1);
        character.clothingMask=hideout.clothingMask;
    }
    if (exitMenuOpen && *exitMenuOpen)
        DrawHideoutExitMenu(*exitMenuOpen,io.DisplaySize);
    ImGui::End();
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(8);
    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RaidHud::RenderScavDialogue(Hideout& hideout,int profileIndex,bool& panelOpen)
{
    if(profileIndex<0||profileIndex>=static_cast<int>(hideout.scavProfiles.size())) { panelOpen=false;return; }
    const int questId=401+profileIndex;
    const auto profile=hideout.scavProfiles[profileIndex];
    const auto defs=hideout.trading.QuestDefs();
    const auto questProgress=hideout.trading.QuestProgresses();
    const auto defIt=std::find_if(defs.begin(),defs.end(),
        [&](const auto& q){return q.id==questId;});
    const auto progressIt=std::find_if(questProgress.begin(),questProgress.end(),
        [&](const auto& p){return p.questId==questId;});
    ImGui::SetNextWindowSize({540,360},ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Always,{.5f,.5f});
    if(ImGui::Begin(profile.name,&panelOpen,ImGuiWindowFlags_NoCollapse)) {
        if(defIt!=defs.end()&&progressIt!=questProgress.end()) {
            const auto& def=*defIt;
            const auto& progress=*progressIt;
            const char* line=progress.status==Trading::QuestStatus::Available?def.intro.c_str():
                progress.status==Trading::QuestStatus::Active?def.active.c_str():
                progress.status==Trading::QuestStatus::ReadyToComplete?def.complete.c_str():"고마워요. 덕분에 한결 나아졌어요.";
            ImGui::TextWrapped("\"%s\"",line);
            ImGui::Spacing();ImGui::Separator();
            ImGui::Text("개인 임무: %s",def.title.c_str());
            ImGui::TextWrapped("%s",def.description.c_str());
            ImGui::Text("진행도: %d / %d",progress.count,def.required);
            if(progress.status==Trading::QuestStatus::Available) {
                if(ImGui::Button("개인 임무 수락"))hideout.trading.AcceptQuest(questId,hideout);
            } else if(progress.status==Trading::QuestStatus::Active&&def.objective==Trading::Objective::DeliverItem) {
                if(ImGui::Button("물품 전달"))hideout.trading.DeliverQuest(questId,hideout);
            } else if(progress.status==Trading::QuestStatus::ReadyToComplete) {
                if(ImGui::Button("완료 및 보상 받기"))hideout.trading.CompleteQuest(questId,hideout);
            } else if(progress.status==Trading::QuestStatus::Completed) ImGui::TextUnformatted("완료됨");
        }
        ImGui::Separator();
        if(profile.deployLocked)ImGui::TextColored({.95f,.45f,.4f,1},"출격 대기 중 · PMC 출격 후 다시 선택할 수 있습니다.");
        if(ImGui::Button("이 SCAV로 전환",{180,34})) {
            if(hideout.SelectScav(profileIndex))panelOpen=false;
        }
        ImGui::SameLine();
        if(ImGui::Button("대화 종료",{140,34}))panelOpen=false;
    }
    ImGui::End();
}

void RaidHud::RenderTrading(Hideout& hideout,Trading::TradeSession& session,bool& panelOpen)
{
    if(!rendererReady)return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    struct DragPayload { int source; int index; };
    auto beginDrag=[](int source,int index) {
        DragPayload payload{source,index};
        if(ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("TARKOV_TRADE_ITEM",&payload,sizeof(payload));
            ImGui::TextUnformatted(source==0?"상인 물품":"내 창고 물품");
            ImGui::EndDragDropSource();
        }
    };
    const auto traderName=[&](int i){return hideout.trading.GetTrader(static_cast<Trading::TraderId>(i)).name.c_str();};
    const auto displayName=[](const ItemStack& item){return GetItemDefinition(item.type).name;};

    ImGui::PushStyleColor(ImGuiCol_WindowBg,{.035f,.043f,.039f,1});
    ImGui::PushStyleColor(ImGuiCol_ChildBg,{.055f,.065f,.057f,1});
    ImGui::PushStyleColor(ImGuiCol_Button,{.16f,.18f,.15f,1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.32f,.33f,.24f,1});
    ImGui::PushStyleColor(ImGuiCol_Border,{.28f,.30f,.24f,1});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,8});
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("상인 거래",&panelOpen,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
    ImGui::TextColored({.78f,.68f,.42f,1},"거래소");
    ImGui::SameLine(ImGui::GetWindowWidth()-85);if(ImGui::Button("뒤로",{65,24}))panelOpen=false;
    for(int i=0;i<Trading::TraderCount;++i) {
        if(i)ImGui::SameLine();
        ImGui::PushID(i);
        ImGui::BeginGroup();
        ImGui::Image(static_cast<ImTextureID>(traderPortraits[static_cast<std::size_t>(i)].id),{96,96});
        ImGui::SameLine();
        if(ImGui::Button(traderName(i),{150,96})) {
            if(session.trader!=static_cast<Trading::TraderId>(i)) {
                hideout.trading.Rollback(session,hideout.storage);
                session.open=true;session.trader=static_cast<Trading::TraderId>(i);session.message.clear();
            }
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::SameLine();
    const bool tradeTabActive=session.tab==Trading::Tab::Trade;
    if(tradeTabActive)ImGui::PushStyleColor(ImGuiCol_Button,{.43f,.39f,.27f,1});
    if(ImGui::Button("거래",{76,34}))session.tab=Trading::Tab::Trade;
    if(tradeTabActive)ImGui::PopStyleColor();
    ImGui::SameLine();
    const bool questTabActive=session.tab==Trading::Tab::Quests;
    if(questTabActive)ImGui::PushStyleColor(ImGuiCol_Button,{.43f,.39f,.27f,1});
    if(ImGui::Button("임무",{76,34})) { hideout.trading.Rollback(session,hideout.storage); session.tab=Trading::Tab::Quests; }
    if(questTabActive)ImGui::PopStyleColor();
    ImGui::Separator();
    auto& currentTrader=hideout.trading.GetTrader(session.trader);
    if(session.tab==Trading::Tab::Quests) {
        ImGui::Text("%s · 충성도 %d",currentTrader.name.c_str(),currentTrader.loyalty);
        ImGui::BeginChild("##TraderQuestList",{0,ImGui::GetContentRegionAvail().y-55},true);
        const auto questDefs=hideout.trading.QuestDefs();
        const auto questProgresses=hideout.trading.QuestProgresses();
        for(const auto& def:questDefs) {
            if(def.trader!=session.trader||def.personalOwner>=0)continue;
            const auto pIt=std::find_if(questProgresses.begin(),questProgresses.end(),[&](const auto& p){return p.questId==def.id;});
            if(pIt==questProgresses.end())continue;
            const auto& p=*pIt;
            ImGui::PushID(def.id);ImGui::TextUnformatted(def.title.c_str());ImGui::TextWrapped("%s",def.description.c_str());
            ImGui::Text("진행도 %d / %d · 보상 %d RUB + %s x%d",p.count,def.required,def.rewardRubles,displayName({def.rewardItem,def.rewardItemCount}),def.rewardItemCount);
            if(p.status==Trading::QuestStatus::Available) { if(ImGui::Button("수락"))hideout.trading.AcceptQuest(def.id,hideout); }
            else if(p.status==Trading::QuestStatus::Active&&def.objective==Trading::Objective::DeliverItem) { if(ImGui::Button("창고 물품 납품"))hideout.trading.DeliverQuest(def.id,hideout); }
            else if(p.status==Trading::QuestStatus::Active) ImGui::TextUnformatted("PMC 레이드에서 목표를 수행하세요.");
            else if(p.status==Trading::QuestStatus::ReadyToComplete) { if(ImGui::Button("완료 및 보상 수령"))hideout.trading.CompleteQuest(def.id,hideout); }
            else ImGui::TextUnformatted("완료됨");
            ImGui::Separator();ImGui::PopID();
        }
        ImGui::EndChild();
    } else {
        const float areaHeight=(std::max)(220.f,ImGui::GetContentRegionAvail().y-30.f);
        const float totalWidth=ImGui::GetContentRegionAvail().x;
        const float outerWidth=(totalWidth-24.f)*.35f;
        const float middleWidth=totalWidth-outerWidth*2.f-24.f;
        const auto texture=static_cast<ImTextureID>(itemImages.id);
        auto drawInventory=[&](const char* id,Inventory& inventory,int source,float panelWidth) {
            ImGui::BeginChild(id,{panelWidth,areaHeight},true);
            ImGui::TextUnformatted(source==0?"상점 재고":"창고");
            ImGui::SameLine();
            if(source==0)ImGui::TextDisabled("충성도 %d",currentTrader.loyalty);
            else ImGui::TextColored({.8f,.76f,.55f,1},"%lld RUB",static_cast<long long>(hideout.rubles));
            ImGui::Separator();
            ImGui::BeginChild("##StockScroll",{0,0},false,ImGuiWindowFlags_HorizontalScrollbar);
            const float cell=(std::max)(25.f,(ImGui::GetContentRegionAvail().x-2.f)/10.f);
            const auto origin=ImGui::GetCursorScreenPos();
            auto* draw=ImGui::GetWindowDrawList();
            const int rows=(std::max)(8,(inventory.SlotCapacity()+9)/10);
            for(int row=0;row<rows;++row)for(int col=0;col<10;++col) {
                const ImVec2 p(origin.x+col*cell,origin.y+row*cell);
                draw->AddRectFilled(p,{p.x+cell,p.y+cell},((row+col)&1)?IM_COL32(26,32,28,255):IM_COL32(22,28,24,255));
                draw->AddRect(p,{p.x+cell,p.y+cell},IM_COL32(63,68,57,150));
            }
            const auto items=inventory.Items();
            for(int i=0;i<static_cast<int>(items.size());++i) {
                const auto& item=items[i];
                if(item.slot<0)continue;
                const auto footprint=ItemFootprint(item.type);
                const ImVec2 p(origin.x+(item.slot%10)*cell,origin.y+(item.slot/10)*cell);
                const ImVec2 size(footprint.width*cell-2,footprint.height*cell-2);
                ImGui::SetCursorScreenPos(p);ImGui::PushID(i);
                ImGui::InvisibleButton("##StockItem",size);
                InventoryUi::Picture(draw,texture,item.type,p,size);
                draw->AddRect(p,{p.x+size.x,p.y+size.y},IM_COL32(119,122,95,190));
                const auto price=std::to_string(hideout.trading.Price(item));
                draw->AddText({p.x+2,p.y+1},IM_COL32(235,229,202,255),price.c_str());
                const auto quantity=std::to_string(item.quantity);
                draw->AddText({p.x+size.x-ImGui::CalcTextSize(quantity.c_str()).x-3,p.y+size.y-ImGui::GetFontSize()},IM_COL32(238,238,220,255),quantity.c_str());
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s x%d\n%s RUB\n거래판으로 드래그",displayName(item),item.quantity,price.c_str());
                beginDrag(source,i);ImGui::PopID();
            }
            ImGui::SetCursorScreenPos(origin);ImGui::Dummy({cell*10,cell*rows});
            ImGui::EndChild();ImGui::EndChild();
        };
        drawInventory("##MyStashTrade",hideout.storage,1,outerWidth);
        ImGui::SameLine(0,12);
        ImGui::BeginChild("##TradeStage",{middleWidth,areaHeight},true);
        ImGui::TextColored({.85f,.79f,.51f,1},"거래 대기");
        ImGui::Separator();
        const int buyTotal=hideout.trading.BuyTotal(session),sellTotal=hideout.trading.SellTotal(session);
        ImGui::Text("구매 %d  /  판매 %d RUB",buyTotal,sellTotal);
        ImGui::Text("정산 후 %lld RUB",static_cast<long long>(hideout.rubles)-buyTotal+sellTotal);
        if(ImGui::Button("거래하기",{-1,36}))hideout.trading.Commit(session,hideout);
        if(ImGui::Button("모두 원위치",{-1,26}))hideout.trading.Rollback(session,hideout.storage);
        ImGui::BeginChild("##StageScroll",{0,0},false);
        auto drawStageGrid=[&](const char* label,bool buyGrid) {
            ImGui::TextUnformatted(label);
            const float cell=(std::max)(24.f,(ImGui::GetContentRegionAvail().x-4.f)/Trading::StageColumns);
            const auto origin=ImGui::GetCursorScreenPos();
            auto* draw=ImGui::GetWindowDrawList();
            for(int y=0;y<Trading::StageRows;++y)for(int x=0;x<Trading::StageColumns;++x) {
                ImGui::PushID((buyGrid?4000:5000)+y*Trading::StageColumns+x);
                const ImVec2 p(origin.x+x*cell,origin.y+y*cell);
                ImGui::SetCursorScreenPos(p);ImGui::InvisibleButton("##tradeCell",{cell,cell});
                draw->AddRectFilled(p,{p.x+cell,p.y+cell},((x+y)&1)?IM_COL32(29,35,28,255):IM_COL32(23,29,24,255));
                draw->AddRect(p,{p.x+cell,p.y+cell},IM_COL32(71,75,58,180));
                if(ImGui::BeginDragDropTarget()) {
                    if(const auto* payload=ImGui::AcceptDragDropPayload("TARKOV_TRADE_ITEM")) {
                        if(payload->DataSize==sizeof(DragPayload)) {
                            const auto data=*static_cast<const DragPayload*>(payload->Data);
                            if(buyGrid&&data.source==0)hideout.trading.StageFromTrader(session,data.index,x,y);
                            else if(!buyGrid&&data.source==1)hideout.trading.StageFromPlayer(session,hideout,data.index,x,y);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::PopID();
            }
            const auto staged=buyGrid?session.buy:session.sell;
            for(int i=0;i<static_cast<int>(staged.size());++i) {
                const auto& item=staged[i];const auto footprint=ItemFootprint(item.stack.type);
                const ImVec2 p(origin.x+item.stageX*cell,origin.y+item.stageY*cell);
                const ImVec2 size(footprint.width*cell-2,footprint.height*cell-2);
                InventoryUi::Picture(draw,texture,item.stack.type,p,size);
                draw->AddRect(p,{p.x+size.x,p.y+size.y},IM_COL32(177,164,102,255));
                const auto quantity=std::to_string(item.stack.quantity);
                draw->AddText({p.x+3,p.y+3},IM_COL32(240,236,207,255),quantity.c_str());
                const auto mouse=ImGui::GetIO().MousePos;
                if(ImGui::IsWindowHovered()&&mouse.x>=p.x&&mouse.x<p.x+size.x&&mouse.y>=p.y&&mouse.y<p.y+size.y) {
                    ImGui::SetTooltip("%s x%d\n우클릭: 원위치",displayName(item.stack),item.stack.quantity);
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        if(buyGrid)hideout.trading.UnstageBuy(session,i);
                        else hideout.trading.UnstageSell(session,hideout.storage,i);
                        break;
                    }
                }
            }
            ImGui::SetCursorScreenPos(origin);ImGui::Dummy({cell*Trading::StageColumns,cell*Trading::StageRows});
        };
        drawStageGrid("구매 · 상점 → 창고",true);
        ImGui::Spacing();drawStageGrid("판매 · 창고 → 상점",false);
        ImGui::EndChild();ImGui::EndChild();
        ImGui::SameLine(0,12);
        drawInventory("##TraderStock",currentTrader.stash,0,0);
    }
    if(!session.message.empty())ImGui::TextWrapped("%s",session.message.c_str());
    ImGui::End();
    ImGui::PopStyleVar(2);ImGui::PopStyleColor(5);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RaidHud::RenderHideoutStorage(Hideout& hideout, int room, int furniture, Player& character,
    CharacterVisual& visual, Shader& shader, bool& panelOpen, bool& exitMenuOpen, const CollisionWorld& world)
{
    if (!panelOpen || room<0 || room>=Hideout::HideoutRoomCount ||
        furniture<0 || furniture>Hideout::PelicanStorage || (furniture==Hideout::PelicanStorage && room!=0)) return;
    RenderHideout(hideout,character,visual,shader,true,&exitMenuOpen,room,furniture,&world);
}

void RaidHud::RenderHideoutPauseMenu(bool& exitMenuOpen)
{
    if (!rendererReady || !exitMenuOpen) return;
    DrawHideoutExitMenu(exitMenuOpen,ImGui::GetIO().DisplaySize);
}
