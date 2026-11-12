#include "RaidInventoryCommands.h"
#include "InventoryActions.h"
#include "../Character/Player.h"
#include "../Combat/LootSystem.h"
#include "../Combat/CombatSystem.h"
#include <utility>
namespace InventoryCommands {
void ApplyRaid(Command command, Player& player, LootSystem& loot, const CollisionWorld& world, InventoryActions& actions, Enemy* corpse) {
    auto& inventory=player.GetInventory();
            bool ok=false;
            if (command.item.source==2 && command.action==Action::Move && command.destination==3 &&
                command.item.id==static_cast<StackId>(ClothingSlot(ItemType::Backpack)+1)) {
                auto nextInventory=inventory;
                auto nextClothing=player.clothingMask;
                auto nextLoot=loot;
                std::vector<ItemStack> removed;
                ok=nextInventory.DetachBackpack(nextClothing,removed);
                if (ok) for (const auto& item:removed)
                    if (!nextLoot.DropNear(player.position,item,world)) { ok=false; break; }
                if (ok) {
                    inventory=std::move(nextInventory);
                    player.clothingMask=nextClothing;
                    loot=std::move(nextLoot);
                    loot.Notify("가방과 내용물을 바닥에 내려놓았습니다.");
                }
            } else if (command.item.source==3 && command.action==InventoryCommands::Action::Move && command.destination==0)
                ok=loot.TakeNearby(command.item.id,player,world,command.slot);
            else if (command.item.source==3 && command.action==InventoryCommands::Action::Use)
                ok=loot.EquipNearby(command.item.id,player,world);
            else if (command.item.source==3 && command.action==InventoryCommands::Action::Move && command.destination==2)
                ok=loot.EquipNearby(command.item.id,player,world,command.slot);
            else if (command.item.source==4 && command.destination==0 && corpse && corpse->rifle) {
                ItemStack weapon{ItemType::Rifle,1,0,30,true,command.slot};
                ok=command.slot==-100?inventory.Pickup(weapon):inventory.TryAdd(weapon);
                if (ok) corpse->rifle=false;
            } else if (!corpse && command.item.source==0 && command.action==InventoryCommands::Action::Move && (command.destination==1 || command.destination==3)) {
                actions.selected=command.item.id; actions.Request(InventoryAction::DropAll); ok=true;
            } else if (command.action==InventoryCommands::Action::Use && command.item.source==0 && inventory.Find(command.item.id) && inventory.Find(command.item.id)->type==ItemType::Bandage) {
                actions.selected=command.item.id; actions.Request(InventoryAction::Use); ok=true;
            } else if (command.item.source==5 && corpse && (command.destination==0 || command.destination==2)) {
                const auto* worn=corpse->wornEquipment.Find(command.item.id);
                if (worn) {
                    const auto remaining=corpse->body.clothingMask & ~ClothingBit(worn->type);
                    auto pockets=corpse->inventory;
                    command.item.source=1;
                    if (pockets.ConfigureGear(remaining) && InventoryCommands::Execute(command,inventory,&corpse->wornEquipment,
                            player.clothingMask,player.vitals,player.appearance==PlayerAppearance::Male)) {
                        corpse->inventory=std::move(pockets);
                        corpse->body.clothingMask=remaining;
                        ok=true;
                    }
                }
            } else {
                ok=InventoryCommands::Execute(command,inventory,corpse?&corpse->inventory:nullptr,player.clothingMask,player.vitals,player.appearance == PlayerAppearance::Male);
            }
            if (!ok) loot.Notify("이동/사용 불가: 공간, 장비 종류 또는 탄약을 확인하세요.");
}
}
