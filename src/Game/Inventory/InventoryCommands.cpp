#include "InventoryCommands.h"
#include <algorithm>
#include <utility>
namespace InventoryCommands {
bool Execute(Command command, Inventory& carried, Inventory* external, std::uint32_t& clothing, PlayerVitals& vitals, bool male)
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
        if (item->type == ItemType::Food) { if (!vitals.Eat()) return false; return source->Remove(item->id,1); }
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
