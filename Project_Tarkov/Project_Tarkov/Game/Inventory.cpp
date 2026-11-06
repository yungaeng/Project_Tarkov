#include "Inventory.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

const ItemDefinition& GetItemDefinition(ItemType type)
{
    static const ItemDefinition bandage{ "붕대", 5, 0.1f };
    static const ItemDefinition water{ "물", 2, 0.6f };
    static const ItemDefinition ammo{ "공용 탄약", 30, 0.01f };
    static const ItemDefinition shirt{ "멀티캠 전투 상의", 1, 0.4f };
    static const ItemDefinition trousers{ "멀티캠 전투 하의", 1, 0.6f };
    static const ItemDefinition vest{ "멀티캠 숏 전술조끼", 1, 1.2f };
    static const ItemDefinition helmet{ "멀티캠 방탄헬멧", 1, 1.3f };
    static const ItemDefinition backpack{ "전술 배낭", 1, 1.5f };
    static const ItemDefinition underShirt{ "검은 스포츠 브라", 1, 0.15f };
    static const ItemDefinition underpants{ "검은 티팬티", 1, 0.1f };
    static const ItemDefinition boots{ "검정색 군화", 1, 1.4f };
    static const ItemDefinition rifle{"소총", 1, 3.5f};
    static const ItemDefinition magazine{"소총 탄창", 1, .25f};
    static const ItemDefinition pistol{"권총", 1, .9f};
    static const ItemDefinition pistolMagazine{"권총 탄창", 1, .12f};
    switch (type)
    {
    case ItemType::Bandage: return bandage;
    case ItemType::Water: return water;
    case ItemType::Ammo: return ammo;
    case ItemType::Rifle: return rifle;
    case ItemType::Magazine: return magazine;
    case ItemType::Pistol: return pistol;
    case ItemType::PistolMagazine: return pistolMagazine;
    case ItemType::Shirt: return shirt;
    case ItemType::Trousers: return trousers;
    case ItemType::Vest: return vest;
    case ItemType::Helmet: return helmet;
    case ItemType::Backpack: return backpack;
    case ItemType::Boots: return boots;
    case ItemType::UnderShirt: return underShirt;
    case ItemType::Trunks: { static const ItemDefinition trunks{"트렁크 팬티",1,0.1f}; return trunks; }
    case ItemType::Underpants: return underpants;
    }
    throw std::invalid_argument("Unknown item type");
}

bool Inventory::TryAdd(ItemStack stack)
{
    if (stack.quantity <= 0 || static_cast<int>(stack.type) < 0 || stack.type >= ItemType::Count) return false;
    if (stack.rounds < 0 || stack.rounds > MagazineCapacity(stack.type) ||
        (!IsWeapon(stack.type) && stack.magazineInserted) ||
        (!IsWeapon(stack.type) && !IsMagazine(stack.type) && stack.rounds) ||
        (IsWeapon(stack.type) && !stack.magazineInserted && stack.rounds)) return false;
    const int limit = GetItemDefinition(stack.type).maxStack;
    // Work on a copy so failed pickups never partially consume a world item.
    auto next = *this;
    auto availableId = nextId;
    for (auto& entry : next.items)
    {
        if (entry.type != stack.type || limit == 1 || (stack.slot != -100 && stack.slot != entry.slot)) continue;
        const int amount = (std::min)(limit - entry.quantity, stack.quantity);
        entry.quantity += amount;
        stack.quantity -= amount;
        if (stack.quantity == 0) break;
    }
    while (stack.quantity > 0)
    {
        int slot = stack.slot;
        if (slot == -100) {
            for (slot = 0; slot < (storage ? slotCapacity : 30); ++slot)
                if (next.CanPlace(stack.type, slot)) break;
        }
        if (!next.CanPlace(stack.type, slot)) return false;
        const int amount = (std::min)(limit, stack.quantity);
        auto entry = stack;
        entry.quantity = amount; entry.id = availableId++; entry.slot = slot;
        next.items.push_back(entry);
        stack.quantity -= amount;
    }
    if (stack.quantity > 0) return false;
    *this = std::move(next);
    nextId = availableId;
    return true;
}

const ItemStack* Inventory::Find(StackId id) const
{
    for (const auto& item : items) if (item.id == id) return &item;
    return nullptr;
}

bool Inventory::Remove(StackId id, int quantity)
{
    for (auto it = items.begin(); it != items.end(); ++it)
        if (it->id == id && quantity > 0 && quantity <= it->quantity)
        {
            it->quantity -= quantity;
            if (!it->quantity) items.erase(it);
            return true;
        }
    return false;
}

int Inventory::Count(ItemType type) const
{
    int total = 0;
    for (const auto& item : items) if (item.type == type) total += item.quantity;
    return total;
}

int Inventory::Take(ItemType type, int quantity)
{
    int taken = 0;
    for (auto it = items.begin(); it != items.end() && taken < quantity;)
    {
        if (it->type != type) { ++it; continue; }
        const int amount = (std::min)(quantity - taken, it->quantity);
        taken += amount;
        it->quantity -= amount;
        if (!it->quantity) it = items.erase(it); else ++it;
    }
    return taken;
}

bool Inventory::SetSlotCapacity(int capacity)
{
    if (capacity < BaseInventorySlots || capacity > 1000000) return false;
    if (capacity <= 30) {
        const auto mask = capacity == 30 ? ClothingBit(ItemType::Vest) | ClothingBit(ItemType::Backpack) :
            capacity == 24 ? ClothingBit(ItemType::Backpack) : capacity == 18 ? ClothingBit(ItemType::Vest) : 0;
        return ConfigureGear(mask);
    }
    for (const auto& item : items) if (item.slot < 0 || item.slot >= capacity) return false;
    slotCapacity = capacity;
    storage = true;
    return true;
}

float Inventory::TotalWeight() const
{
    float weight = 0;
    for (const auto& entry : items)
        weight += GetItemDefinition(entry.type).weight * entry.quantity + entry.rounds * .01f +
            (IsWeapon(entry.type) && entry.magazineInserted ? GetItemDefinition(MagazineType(entry.type)).weight : 0);
    return weight;
}

bool Inventory::EquipClothing(StackId id, std::uint32_t& equipped)
{
    const auto* stack = Find(id);
    if (!stack || !IsClothing(stack->type)) return false;
    const auto bit = ClothingBit(stack->type);
    if ((equipped & bit) || !Remove(id, 1)) return false;
    equipped |= bit;
    SetSlotCapacity(InventorySlotsForGear(equipped));
    return true;
}

bool Inventory::UnequipClothing(ItemType type, std::uint32_t& equipped)
{
    const auto bit = ClothingBit(type);
    if (!bit || !(equipped & bit)) return false;
    const auto next = equipped & ~bit;
    auto candidate = *this;
    if (!candidate.ConfigureGear(next) || !candidate.TryAdd({type, 1})) return false;
    *this = std::move(candidate);
    equipped = next;
    return true;
}

bool Inventory::SlotAllowed(ItemType type, int slot) const
{
    if (slot < 0) return !storage && ((slot==-1 || slot==-2) ? type==ItemType::Rifle : slot==-3 && type==ItemType::Pistol);
    if (!storage && ((slot>=12 && slot<18 && !(gear&ClothingBit(ItemType::Vest))) ||
        (slot>=18 && slot<30 && !(gear&ClothingBit(ItemType::Backpack))))) return false;
    const int end=storage?slotCapacity:slot<12?12:slot<18?18:30;
    if (slot>=end) return false;
    const auto size=ItemFootprint(type);
    const int columns=Columns(slot),local=slot-RegionStart(slot);
    return local%columns+size.width<=columns && slot+(size.height-1)*columns+size.width<=end;
}
const ItemStack* Inventory::Covering(int slot) const
{
    if (slot<0) return At(slot);
    for (const auto& item:items) {
        if (item.slot<0 || RegionStart(item.slot)!=RegionStart(slot)) continue;
        const int columns=Columns(slot),base=RegionStart(slot);
        const auto size=ItemFootprint(item.type);
        const int x=(slot-base)%columns,y=(slot-base)/columns;
        const int ix=(item.slot-base)%columns,iy=(item.slot-base)/columns;
        if (x>=ix && x<ix+size.width && y>=iy && y<iy+size.height) return &item;
    }
    return nullptr;
}
bool Inventory::CanPlace(ItemType type,int slot,StackId ignore) const
{
    if (!SlotAllowed(type,slot)) return false;
    const auto size=slot<0?ItemSize{1,1}:ItemFootprint(type);
    for (int y=0;y<size.height;++y) for (int x=0;x<size.width;++x) {
        const auto* occupied=Covering(slot+y*Columns(slot)+x);
        if (occupied && occupied->id!=ignore) return false;
    }
    return true;
}
const ItemStack* Inventory::At(int slot) const
{
    for (const auto& item : items) if (item.slot == slot) return &item;
    return nullptr;
}
ItemStack* Inventory::Edit(StackId id)
{
    for (auto& item : items) if (item.id == id) return &item;
    return nullptr;
}
int Inventory::StorageUsed() const
{
    int count = 0;
    for (const auto& item : items) if (item.slot >= 0) { const auto size=ItemFootprint(item.type); count+=size.width*size.height; }
    return count;
}
bool Inventory::ConfigureGear(std::uint32_t clothing)
{
    auto next = *this;
    next.gear = clothing;
    next.storage = false;
    next.slotCapacity = InventorySlotsForGear(clothing);
    for (const auto& item : next.items) if (!next.SlotAllowed(item.type, item.slot)) return false;
    *this = std::move(next);
    return true;
}
bool Inventory::Pickup(ItemStack stack)
{
    if (IsWeapon(stack.type)) {
        for (int slot : {-1, -2, -3})
            if (SlotAllowed(stack.type, slot) && !At(slot)) { stack.slot = slot; return TryAdd(stack); }
    }
    stack.slot = -100;
    return TryAdd(stack);
}
bool Inventory::Move(StackId id, int slot)
{
    auto* item = Edit(id);
    if (item && slot == -100) {
        for (slot = 0; slot < (storage ? slotCapacity : 30); ++slot)
            if (CanPlace(item->type, slot, id)) break;
    }
    if (!item || !SlotAllowed(item->type, slot)) return false;
    auto* other = At(slot) ? Edit(At(slot)->id) : nullptr;
    if (other && other->id == id) return true;
    if (other && other->type == item->type && GetItemDefinition(item->type).maxStack > 1) {
        const int amount = (std::min)(item->quantity, GetItemDefinition(item->type).maxStack - other->quantity);
        if (amount > 0) { other->quantity += amount; return Remove(id, amount); }
    }
    auto candidate=*this;
    const int previous=item->slot;
    candidate.Edit(id)->slot=slot;
    if (other) candidate.Edit(other->id)->slot=previous;
    for (const auto& entry:candidate.items)
        if (!candidate.CanPlace(entry.type,entry.slot,entry.id)) return false;
    *this=std::move(candidate);
    return true;
}
bool Inventory::TransferTo(StackId id, Inventory& destination, int slot)
{
    if (&destination == this) return Move(id, slot);
    const auto* item = Find(id);
    if (!item) return false;
    auto copy = *item;
    copy.slot = slot;
    if (!destination.TryAdd(copy)) return false;
    return Remove(id, copy.quantity);
}
bool Inventory::FillMagazine(StackId id, int amount, StackId ammo)
{
    auto* item = Edit(id);
    if (!item || !IsMagazine(item->type) || amount <= 0) return false;
    int available = Count(ItemType::Ammo);
    if (ammo) { const auto* source = Find(ammo); available = source && source->type == ItemType::Ammo ? source->quantity : 0; }
    const int count = (std::min)({amount, MagazineCapacity(item->type) - item->rounds, available});
    if (count <= 0) return false;
    item->rounds += count;
    if (ammo) Remove(ammo, count); else Take(ItemType::Ammo, count);
    return true;
}
bool Inventory::UnloadMagazine(StackId id)
{
    const auto* item = Find(id);
    if (!item || !IsMagazine(item->type) || !item->rounds) return false;
    auto next = *this;
    if (!next.TryAdd({ItemType::Ammo, item->rounds})) return false;
    next.Edit(id)->rounds = 0;
    *this = std::move(next);
    return true;
}
bool Inventory::InsertMagazine(StackId magId, StackId weaponId)
{
    auto* mag = Edit(magId);
    auto* weapon = Edit(weaponId);
    if (!mag || !weapon || !IsWeapon(weapon->type) || mag->type != MagazineType(weapon->type)) return false;
    const int previous = weapon->rounds;
    weapon->rounds = mag->rounds;
    const bool hadMagazine = weapon->magazineInserted;
    weapon->magazineInserted = true;
    if (hadMagazine) mag->rounds = previous;
    else Remove(magId, 1);
    return true;
}
StackId Inventory::BestMagazine(ItemType weapon, std::uint32_t clothing) const
{
    if (!(clothing & ClothingBit(ItemType::Vest))) return 0;
    const ItemStack* best = nullptr;
    for (const auto& item : items)
        if (item.type == MagazineType(weapon) && item.slot >= 12 && item.slot < 18 &&
            (!best || item.rounds > best->rounds)) best = &item;
    return best ? best->id : 0;
}

bool Inventory::EjectMagazine(StackId id)
{
    const auto* weapon=Find(id);
    if (!weapon || !IsWeapon(weapon->type) || !weapon->magazineInserted) return false;
    auto next=*this;
    if (!next.TryAdd({MagazineType(weapon->type),1,0,weapon->rounds})) return false;
    auto* changed=next.Edit(id);
    changed->rounds=0; changed->magazineInserted=false;
    *this=std::move(next);
    return true;
}
void Inventory::AutoArrange()
{
    if (!storage) return;
    auto ordered=items;
    std::stable_sort(ordered.begin(),ordered.end(),[](const ItemStack& a,const ItemStack& b) {
        const auto x=ItemFootprint(a.type),y=ItemFootprint(b.type);
        return x.width*x.height!=y.width*y.height ? x.width*x.height>y.width*y.height : a.type<b.type;
    });
    auto next=*this; next.items.clear();
    for (auto item:ordered) {
        int slot=0;
        while (slot<slotCapacity && !next.CanPlace(item.type,slot)) ++slot;
        if (slot==slotCapacity) return;
        item.slot=slot; next.items.push_back(item);
    }
    *this=std::move(next);
}
