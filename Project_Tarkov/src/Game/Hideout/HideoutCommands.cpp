#include "Hideout.h"
#include "../Inventory/InventoryCommands.h"
#include <utility>
void Hideout::ApplyInventoryCommand(const InventoryCommands::Command& command, int storageRoom, int storageFurniture)
{
    auto& hideout=*this;
    const bool furnitureMode=storageRoom>=0 && storageRoom<HideoutRoomCount && storageFurniture>=0 && storageFurniture<FurnitureCount;
    const auto furnitureKind=static_cast<HideoutFurniture>(storageFurniture);
    const int furnitureSource=furnitureMode ? FurnitureStorageSource(storageRoom,furnitureKind) : -1;
    auto& shownStorage=furnitureMode ? FurnitureStorageAt(storageRoom,furnitureKind) : storage;
    if (command.action!=InventoryCommands::Action::None && hideout.ready) {
        const bool furnitureTransfer=furnitureMode && command.action==InventoryCommands::Action::Move &&
            ((command.item.source==0&&command.destination==furnitureSource) ||
             (command.item.source==furnitureSource&&command.destination==0));
        if (furnitureTransfer) {
            hideout.TransferFurnitureItem(storageRoom,furnitureKind,command.item.id,
                command.item.source==0,command.slot);
        } else if (command.action==InventoryCommands::Action::Sell) {
            if (furnitureMode && command.item.source==furnitureSource)
                hideout.SellFurnitureItem(storageRoom,furnitureKind,command.item.id);
            else
                hideout.SellStashItem(command.item.id);
        } else {
            auto before=hideout;
            bool executed=false;
            if (furnitureMode && command.item.source==furnitureSource) {
                auto localCommand=command;
                if (command.action==InventoryCommands::Action::Use || command.destination==2) {
                    localCommand.item.source=1;
                    executed=InventoryCommands::Execute(localCommand,hideout.loadout,&shownStorage,
                        hideout.clothingMask,hideout.vitals,hideout.appearance==PlayerAppearance::Male);
                } else {
                    localCommand.item.source=0;
                    executed=InventoryCommands::Execute(localCommand,shownStorage,nullptr,hideout.clothingMask,
                        hideout.vitals,hideout.appearance==PlayerAppearance::Male);
                }
            } else {
                executed=InventoryCommands::Execute(command,hideout.loadout,&hideout.storage,hideout.clothingMask,
                    hideout.vitals,hideout.appearance==PlayerAppearance::Male);
            }
            if (!executed)
                hideout.message="이동/사용 불가: 빈 공간, 장비 종류, 탄약을 확인하세요.";
            else if (!hideout.Save()) { const auto failure=hideout.message; hideout=std::move(before); hideout.message=failure; }
        }
    }
}
