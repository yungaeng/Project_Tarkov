#pragma once
#include "Inventory.h"
#include "PlayerVitals.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <utility>

namespace InventoryUi {
inline float PanelOpacity = 1.f;
// A single deferred command avoids mutating containers while ImGui iterates them.
struct Drag { int source; StackId id; };
enum class Action { None, Move, Use, Heal, FillOne, FillAll, Unload, Eject, Sell, Split };
struct Command {
    Action action = Action::None;
    Drag item{};
    int destination = 0;
    int slot = -100;
    int quantity = 0;
};
inline void Picture(ImDrawList* draw, ImTextureID texture, ItemType type, ImVec2 p, ImVec2 size, float opacity = 1.f)
{
    const int cell = static_cast<int>(type);
    const ImVec2 uv0((cell % 4) * .25f, (cell / 4) * .25f);
    const ImVec2 uv1(uv0.x + .25f, uv0.y + .25f);
    draw->AddImage(texture, p, {p.x + size.x, p.y + size.y}, uv0, uv1, IM_COL32(255,255,255,static_cast<int>(255*opacity)));
}
inline void Target(Command& command, int destination, int slot)
{
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload("INVENTORY_OBJECT"))
            if (payload->DataSize == sizeof(Drag)) command = {Action::Move, *static_cast<const Drag*>(payload->Data), destination, slot};
        ImGui::EndDragDropTarget();
    }
}
inline void Cell(const ItemStack* item, int source, int slot, ImTextureID texture, Command& command,
    ImVec2 size, float opacity = 1.f, bool showPrice = false, bool male = false,
    StorageFilter transferFilter = StorageFilter::None, int storageSource = -1)
{
    opacity *= PanelOpacity;
    ImGui::PushID(source); ImGui::PushID(slot);
    const auto p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("slot", size);
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p, {p.x+size.x,p.y+size.y}, IM_COL32(25,29,28,static_cast<int>(150*opacity)));
    if (item) {
        const float side = (std::min)(size.x, size.y);
        Picture(draw, texture, item->type, {p.x+(size.x-side)*.5f,p.y}, {side,side},opacity);
        if (ImGui::BeginDragDropSource()) {
            Drag payload{source,item->id};
            ImGui::SetDragDropPayload("INVENTORY_OBJECT", &payload, sizeof(payload));
            ImGui::TextUnformatted(GetItemDefinition(item->type).name);
            ImGui::EndDragDropSource();
        }
        if (hovered) {
            const auto& definition = GetItemDefinition(item->type);
            if (showPrice)
                ImGui::SetTooltip("%s\n수량 %d | 잔탄 %d/%d%s\n개당 가격 %d RUB | 묶음 가격 %lld RUB",
                    definition.name, item->quantity, item->rounds, MagazineCapacity(item->type),
                    IsWeapon(item->type) && !item->magazineInserted ? " | 탄창 없음" : "",
                    definition.price, static_cast<long long>(definition.price) * item->quantity);
            else
                ImGui::SetTooltip("%s\n수량 %d | 잔탄 %d/%d%s", definition.name,
                    item->quantity, item->rounds, MagazineCapacity(item->type),
                    IsWeapon(item->type) && !item->magazineInserted ? " | 탄창 없음" : "");
        }
        if (ImGui::BeginPopupContextItem("actions")) {
            if (storageSource >= 0) {
                if (source == 0 && MatchesStorageFilter(transferFilter,item->type) &&
                    ImGui::MenuItem("가구 보관함에 보관"))
                    command = {Action::Move,{source,item->id},storageSource};
                if (source == storageSource && ImGui::MenuItem("휴대품으로 꺼내기"))
                    command = {Action::Move,{source,item->id},0};
            } else if (source == 1 && showPrice) {
                if (IsClothing(item->type) && CanWear(item->type,male) &&
                    ImGui::MenuItem("착용하기"))
                    command = {Action::Use,{source,item->id}};
                if (ImGui::MenuItem("판매")) command = {Action::Sell,{source,item->id}};
                if (item->quantity > 1 && GetItemDefinition(item->type).maxStack > 1 &&
                    ImGui::MenuItem("분할 (절반)"))
                    command = {Action::Split,{source,item->id},0,-100,item->quantity/2};
            } else {
                if (source == 2 && ImGui::MenuItem("해제 → 휴대품")) command = {Action::Move,{source,item->id},0,-100};
                if (source != 2 && (item->type == ItemType::Water || IsClothing(item->type)))
                    if (ImGui::MenuItem(IsClothing(item->type) ? "착용" : "사용")) command = {Action::Use,{source,item->id}};
                if (item->type == ItemType::Bandage && ImGui::BeginMenu("치료 부위")) {
                    for (int part=0; part<BodyPartCount; ++part)
                        if (ImGui::MenuItem(BodyPartNames[part])) command={Action::Heal,{source,item->id},0,part};
                    ImGui::EndMenu();
                }
                if (IsMagazine(item->type)) {
                    if (ImGui::MenuItem("1발만 채우기")) command = {Action::FillOne,{source,item->id}};
                    if (ImGui::MenuItem("전부 채우기")) command = {Action::FillAll,{source,item->id}};
                    if (ImGui::MenuItem("전부 빼내기")) command = {Action::Unload,{source,item->id}};
                }
                if (IsWeapon(item->type) && item->magazineInserted && ImGui::MenuItem("탄창 분리"))
                    command={Action::Eject,{source,item->id}};
                if (source == 0 && ImGui::MenuItem("바닥 / 창고로 이동")) command = {Action::Move,{source,item->id},1,-100};
                if ((source == 1 || source == 5) && ImGui::MenuItem("휴대품으로 이동")) command = {Action::Move,{source,item->id},0,-100};
            }
            ImGui::EndPopup();
        }
        std::string amount = IsMagazine(item->type) || IsWeapon(item->type) ?
            (IsWeapon(item->type) && !item->magazineInserted ? "—" : std::to_string(item->rounds)+"/"+std::to_string(MagazineCapacity(item->type))) : std::to_string(item->quantity);
        draw->AddRectFilled({p.x,p.y+size.y-19},{p.x+size.x,p.y+size.y},IM_COL32(10,12,12,static_cast<int>(150*opacity)));
        draw->AddText({p.x+3,p.y+size.y-18},IM_COL32(235,233,211,255),amount.c_str());
    }
    draw->AddRect(p,{p.x+size.x,p.y+size.y},hovered?IM_COL32(214,195,127,255):IM_COL32(91,102,93,255));
    Target(command, source, slot);
    ImGui::PopID(); ImGui::PopID();
}
// Draw one card over the whole footprint, and keep covered tiles out of hit testing.
inline void GridAt(const Inventory& inventory,int source,int first,int count,int columns,
    ImTextureID texture,Command& command,ImVec2 origin,float cell,float gap=4.f,const char* search="",
    bool showPrices=false,bool male=false,StorageFilter transferFilter=StorageFilter::None,
    int storageSource=-1,StorageFilter displayFilter=StorageFilter::None)
{
    for (int i=0;i<count;++i) {
        if (inventory.Covering(first+i)) continue;
        ImGui::SetCursorScreenPos({origin.x+(i%columns)*(cell+gap),origin.y+(i/columns)*(cell+gap)});
        Cell(nullptr,source,first+i,texture,command,{cell,cell},1.f,false,false,transferFilter,storageSource);
    }
    for (const auto& item:inventory.Items()) {
        if (displayFilter != StorageFilter::None && !MatchesStorageFilter(displayFilter,item.type)) continue;
        if (item.slot<first || item.slot>=first+count) continue;
        const int local=item.slot-first;
        const auto footprint=ItemFootprint(item.type);
        ImGui::SetCursorScreenPos({origin.x+(local%columns)*(cell+gap),origin.y+(local/columns)*(cell+gap)});
        const bool match=!search[0] || std::string(GetItemDefinition(item.type).name).find(search)!=std::string::npos;
        Cell(&item,source,item.slot,texture,command,
            {footprint.width*cell+(footprint.width-1)*gap,footprint.height*cell+(footprint.height-1)*gap},
            match?1.f:.2f,showPrices,male,transferFilter,storageSource);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({columns*(cell+gap),((count+columns-1)/columns)*(cell+gap)});
}
inline void Grid(const char* title,const Inventory& inventory,int source,int first,int count,int columns,ImTextureID texture,Command& command)
{
    ImGui::PushID(title);
    ImGui::TextUnformatted(title);
    const float cell=(std::max)(26.f,(ImGui::GetContentRegionAvail().x-(columns-1)*4.f)/columns);
    GridAt(inventory,source,first,count,columns,texture,command,ImGui::GetCursorScreenPos(),cell);
    ImGui::PopID();
}
inline void Equipment(const Inventory& inventory, std::uint32_t clothing, ImTextureID texture, Command& command)
{
    ImGui::TextUnformatted("캐릭터 장비 슬롯");
    for (int slot=-1; slot>=-3; --slot) {
        ImGui::Text("%s [%d]",slot==-3?"부무기":"주무기",-slot);
        Cell(inventory.At(slot),0,slot,texture,command,{ImGui::GetContentRegionAvail().x,68},.38f);
    }
    for (int i=0; i<ClothingSlotCount; ++i) {
        const auto type=ClothingType(i);
        ImGui::TextUnformatted(GetItemDefinition(type).name);
        ItemStack worn{type,1,static_cast<StackId>(i+1)};
        Cell((clothing & ClothingBit(type)) ? &worn : nullptr,2,i,texture,command,{ImGui::GetContentRegionAvail().x,58},.38f);
    }
}
inline void Carried(const Inventory& inventory, std::uint32_t clothing, ImTextureID texture, Command& command)
{
    ImGui::Text("휴대품 %d/%d | %.2f kg",inventory.StorageUsed(),inventory.SlotCapacity(),inventory.TotalWeight());
    Grid("주머니",inventory,0,0,12,6,texture,command);
    if (clothing & ClothingBit(ItemType::Vest)) Grid("조끼 · 재장전용 탄창",inventory,0,12,6,3,texture,command);
    else ImGui::TextDisabled("조끼 미착용");
    if (clothing & ClothingBit(ItemType::Backpack)) Grid("배낭",inventory,0,18,12,4,texture,command);
    else ImGui::TextDisabled("배낭 미착용");
}
inline bool Execute(Command command, Inventory& carried, Inventory* external, std::uint32_t& clothing, PlayerVitals& vitals, bool male=false)
{
    if (command.action == Action::None) return true;
    if (command.action == Action::Sell) return false;
    if (command.item.source == 2) {
        if (command.action != Action::Move || command.item.id < 1 || command.item.id > ClothingSlotCount) return false;
        const auto type = ClothingType(static_cast<int>(command.item.id)-1,male);
        const auto bit = ClothingBit(type);
        if (!(clothing & bit)) return false;
        auto next = carried;
        if (!next.ConfigureGear(clothing & ~bit)) return false;
        ItemStack removed{type,1}; removed.slot=command.slot;
        if (command.destination == 0) {
            if (!next.TryAdd(removed)) return false;
        } else if (command.destination == 1 && external) {
            if (!external->TryAdd(removed)) return false;
        } else return false;
        carried=std::move(next); clothing &= ~bit;
        return true;
    }
    Inventory* source=command.item.source==0 ? &carried : command.item.source==1 ? external : nullptr;
    if (!source) return false;
    const auto* item=source->Find(command.item.id);
    if (!item) return false;
    if (command.action == Action::Split)
        return source->SplitStack(item->id,command.quantity);
    if (command.action == Action::Heal) {
        if (item->type != ItemType::Bandage || command.slot < 0 || command.slot >= BodyPartCount ||
            !vitals.Heal(static_cast<BodyPart>(command.slot))) return false;
        return source->Remove(item->id,1);
    }
    if (command.action == Action::FillOne || command.action == Action::FillAll)
        return source->FillMagazine(item->id, command.action == Action::FillOne ? 1 : 30);
    if (command.action == Action::Unload) return source->UnloadMagazine(item->id);
    if (command.action == Action::Eject) return source->EjectMagazine(item->id);
    if (command.action == Action::Use) {
        if (item->type == ItemType::Water) { if (!vitals.Drink()) return false; return source->Remove(item->id,1); }
        if (!CanWear(item->type,male) || !IsClothing(item->type)) return false;
        if (source == &carried) return carried.EquipClothing(item->id,clothing);
        auto next = carried;
        const auto bit=ClothingBit(item->type);
        auto nextSource = *source;
        if (clothing & bit) {
            const auto replaced = ClothingType(ClothingSlot(item->type),male);
            if (!external || external != source || !nextSource.Remove(item->id,1) ||
                !nextSource.TryAdd({replaced,1}))
                return false;
            *source = std::move(nextSource);
            return true;
        }
        if (!next.ConfigureGear(clothing | bit) || !nextSource.Remove(item->id,1)) return false;
        clothing |= bit;
        carried=std::move(next);
        *source=std::move(nextSource);
        return true;
    }
    if (command.destination == 2) {
        if (!IsClothing(item->type) || item->type != ClothingType(command.slot,male)) return false;
        command.action=Action::Use; return Execute(command,carried,external,clothing,vitals,male);
    }
    Inventory* destination=command.destination==0 ? &carried : command.destination==1 ? external : nullptr;
    if (!destination) return false;
    const auto* target=destination->Covering(command.slot);
    if (target && source != destination) {
        if (item->type == ItemType::Ammo && IsMagazine(target->type)) {
            const int amount=(std::min)(item->quantity,MagazineCapacity(target->type)-target->rounds);
            if (amount<=0) return false;
            destination->Edit(target->id)->rounds += amount;
            return source->Remove(item->id,amount);
        }
        if (IsMagazine(item->type) && IsWeapon(target->type) && item->type==MagazineType(target->type)) {
            auto* weapon=destination->Edit(target->id);
            const int previous=weapon->rounds;
            weapon->rounds=item->rounds;
            const bool hadMagazine=weapon->magazineInserted;
            weapon->magazineInserted=true;
            if (hadMagazine) source->Edit(item->id)->rounds=previous;
            else source->Remove(item->id,1);
            return true;
        }
    }
    if (target && source == destination && target->id != item->id) {
        if (item->type == ItemType::Ammo && IsMagazine(target->type)) return source->FillMagazine(target->id,30,item->id);
        if (IsMagazine(item->type) && IsWeapon(target->type)) return source->InsertMagazine(item->id,target->id);
    }
    if (command.slot == -100 && destination == &carried && source != destination) {
        auto copy=*item;
        if (!carried.Pickup(copy)) return false;
        return source->Remove(copy.id,copy.quantity);
    }
    return source->TransferTo(item->id,*destination,command.slot);
}
}
