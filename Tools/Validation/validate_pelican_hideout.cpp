#include "../../Project_Tarkov/src/Game/Hideout/Hideout.h"
#include "../../Project_Tarkov/src/Game/Inventory/InventoryCommands.h"
#include "../../Project_Tarkov/src/Game/World/HideoutLayout.h"
#include "../../Project_Tarkov/src/Core/RuntimePaths.h"
#include "../../Project_Tarkov/src/Graphics/Renderer.h"
#include "../../Project_Tarkov/src/Game/World/IndustrialZone.h"
#include <cstdlib>
#include <cassert>
#include <fstream>
#include <iostream>

// This headless test exercises real inventory, persistence and floor placement;
// rendering and raid loot seeding are deliberately unavailable.
const std::array<glm::vec3,5>& IndustrialZone::Buildings() { std::abort(); }
void Renderer::Draw(Shader&, Mesh&, Camera&, glm::mat4, float, float,
    const glm::vec3&, bool, unsigned int, int, float, bool) { std::abort(); }

int main()
{
    // Run only with an explicitly isolated TARKOV_SAVE_ROOT.
    assert(std::getenv("TARKOV_SAVE_ROOT"));
    Hideout old;
    old.ready=true;
    assert(old.storage.SetSlotCapacity(80));
    assert(old.loadout.ConfigureGear(old.clothingMask));
    for (auto& inventory:old.furnitureStorage) assert(inventory.SetSlotCapacity(80));
    assert(old.FurnitureStorageAt(0,HideoutFurniture::Armory).TryAdd({ItemType::Rifle,1,0,17,true}));
    assert(old.FurnitureStorageAt(1,HideoutFurniture::Wardrobe).TryAdd({ItemType::Shirt,1}));
    assert(old.FurnitureStorageAt(1,HideoutFurniture::Refrigerator).TryAdd({ItemType::Water,2}));
    std::filesystem::create_directories(RuntimePaths::SaveRoot());
    {
        std::ofstream out(RuntimePaths::Save("hideout.txt"));
        out << "HIDEOUT 19\n0 " << old.clothingMask << " 0 0 0\n";
        auto write=[&](const Inventory& inventory) {
            out << inventory.SlotCapacity() << ' ' << inventory.Items().size() << '\n';
            for (const auto& item:inventory.Items())
                out << static_cast<int>(item.type) << ' ' << item.quantity << ' ' << item.slot << ' '
                    << item.rounds << ' ' << item.magazineInserted << ' ' << static_cast<int>(item.fireMode) << '\n';
        };
        write(old.storage); write(old.loadout);
        for (const auto& inventory:old.furnitureStorage) write(inventory);
        for (float health:old.vitals.BodyHealth()) out << health << ' ';
        out << old.vitals.Hydration() << ' ' << old.vitals.Stamina() << "\nEND\n";
    }
    Hideout loaded;
    loaded.Load();
    assert(loaded.ready && loaded.storage.Count(ItemType::Rifle)==1 && loaded.storage.Count(ItemType::Shirt)==1);
    assert(loaded.FurnitureStorageAt(0,HideoutFurniture::Armory).Items().empty());
    assert(loaded.FurnitureStorageAt(1,HideoutFurniture::Wardrobe).Items().empty());
    assert(loaded.FurnitureStorageAt(1,HideoutFurniture::Refrigerator).Count(ItemType::Water)==2);
    for (const auto& item:loaded.storage.Items())
        if (item.type==ItemType::Rifle) assert(item.rounds==17 && item.magazineInserted);

    loaded.clothingMask |= ClothingBit(ItemType::Backpack);
    assert(loaded.loadout.ConfigureGear(loaded.clothingMask));
    assert(loaded.loadout.TryAdd({ItemType::Magazine,1,0,23,false,18}));
    const InventoryCommands::Command drop{InventoryCommands::Action::Move,
        {2,static_cast<StackId>(ClothingSlot(ItemType::Backpack)+1)},3};
    CollisionWorld world;
    assert(!loaded.DropToFloor(drop,{3,0,-3},world));
    assert(loaded.loadout.At(18) && loaded.floorLoot.WorldItems().empty());
    world.AddBox({{-10,-1,-10},{30,0,10}});
    assert(loaded.DropToFloor(drop,{3,0,-3},world));
    assert(!(loaded.clothingMask & ClothingBit(ItemType::Backpack)) && loaded.floorLoot.WorldItems().size()==2);
    Hideout restored;
    restored.Load();
    assert(restored.ready && restored.floorLoot.WorldItems().size()==2);
    assert(restored.storage.Count(ItemType::Rifle)==1 && restored.storage.Count(ItemType::Shirt)==1);
    for (const auto& item:restored.floorLoot.WorldItems())
        if (item.stack.type==ItemType::Magazine) assert(item.stack.rounds==23);

    std::vector<WorldGeometry::Block> first,second;
    CollisionWorld firstWorld,secondWorld;
    HideoutLayout::AppendFurniture(0,true,firstWorld,first);
    HideoutLayout::AppendFurniture(1,true,secondWorld,second);
    assert(first.size()>second.size());
    const auto anchor=HideoutLayout::Anchor(0,"pelicanCase");
    assert(HideoutLayout::NearestStorage(0,anchor+glm::vec3(0,0,1))==Hideout::PelicanStorage);
    assert(HideoutLayout::NearestStorage(1,anchor+glm::vec3(6,0,1))!=Hideout::PelicanStorage);
    assert(HideoutLayout::NearestStorage(0,anchor-glm::vec3(0,0,1))!=Hideout::PelicanStorage);
    for (int room=0;room<4;++room)
        for (float x=.5f;x<5.6f;x+=.5f)
            for (float z=-7.5f;z<0;z+=.5f) {
                const int storage=HideoutLayout::NearestStorage(room,{room*6.f+x,0,z});
                assert(storage!=1 && storage!=2);
                if (room!=0) assert(storage!=Hideout::PelicanStorage);
            }
    std::cout << "Pelican layout, legacy migration, floor drop and reload checks passed\n";
}
