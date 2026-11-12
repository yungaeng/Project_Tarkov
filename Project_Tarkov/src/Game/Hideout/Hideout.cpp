#include "Hideout.h"
#include "../Inventory/InventoryCommands.h"
#include "../Character/Player.h"
#include <cmath>
#include "../../Core/RuntimePaths.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <utility>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace { constexpr int MaxStored = 1000000; }

bool Hideout::AutoArrangeStash(int room, int furniture)
{
    if (!ready) return false;
    auto before=*this;
    if (room == -1 && furniture == -1) storage.AutoArrange();
    else {
        if (room < 0 || room >= HideoutRoomCount || furniture < 0 || furniture >= FurnitureCount) {
            message = "정리할 보관함을 찾을 수 없습니다.";
            return false;
        }
        FurnitureStorageAt(room,static_cast<HideoutFurniture>(furniture)).AutoArrange();
    }
    if (!Save()) { const auto failure=message; *this=std::move(before); message=failure; return false; }
    return true;
}

bool Hideout::SellStashItem(StackId id)
{
    if (!ready) return false;
    const auto* item = storage.Find(id);
    if (!item) { message = "판매할 창고 아이템이 없습니다."; return false; }
    const auto proceeds = static_cast<std::int64_t>(GetItemDefinition(item->type).price) * item->quantity;
    if (proceeds <= 0 || rubles > (std::numeric_limits<std::int64_t>::max)() - proceeds) {
        message = "판매 금액을 처리할 수 없습니다.";
        return false;
    }
    auto before = *this;
    if (!storage.Remove(id, item->quantity)) {
        message = "창고 아이템 판매에 실패했습니다.";
        return false;
    }
    rubles += proceeds;
    if (!Save()) { const auto failure = message; *this = std::move(before); message = failure; return false; }
    message = "판매 완료: +" + std::to_string(proceeds) + " RUB";
    return true;
}

bool Hideout::TransferFurnitureItem(int room, HideoutFurniture furniture, StackId id, bool store, int slot)
{
    if (!ready) return false;
    if (room < 0 || room >= HideoutRoomCount ||
        static_cast<int>(furniture) < 0 || furniture >= HideoutFurniture::Count) {
        message = "가구 보관함을 찾을 수 없습니다.";
        return false;
    }
    auto updated = *this;
    auto& storage = updated.FurnitureStorageAt(room, furniture);
    auto& source = store ? updated.loadout : storage;
    auto& destination = store ? storage : updated.loadout;
    const auto* item = source.Find(id);
    if (!item || !MatchesStorageFilter(FilterFor(furniture),item->type) ||
        !source.TransferTo(id,destination,slot)) {
        message = store ? "이 가구에는 해당 종류의 아이템만 보관할 수 있습니다." : "꺼낼 아이템이 없거나 휴대 공간이 부족합니다.";
        return false;
    }
    if (!updated.Save()) { message = updated.message; return false; }
    *this = std::move(updated);
    return true;
}

bool Hideout::SellFurnitureItem(int room, HideoutFurniture furniture, StackId id)
{
    if (!ready || room < 0 || room >= HideoutRoomCount ||
        static_cast<int>(furniture) < 0 || furniture >= HideoutFurniture::Count) {
        message = "가구 보관함을 찾을 수 없습니다.";
        return false;
    }
    auto updated = *this;
    auto& inventory = updated.FurnitureStorageAt(room, furniture);
    const auto* item = inventory.Find(id);
    if (!item) { message = "판매할 가구 보관함 아이템이 없습니다."; return false; }
    const auto proceeds = static_cast<std::int64_t>(GetItemDefinition(item->type).price) * item->quantity;
    if (proceeds <= 0 || updated.rubles > (std::numeric_limits<std::int64_t>::max)() - proceeds) {
        message = "판매 금액을 처리할 수 없습니다.";
        return false;
    }
    if (!inventory.Remove(id, item->quantity)) {
        message = "가구 보관함 아이템 판매에 실패했습니다.";
        return false;
    }
    updated.rubles += proceeds;
    if (!updated.Save()) { message = updated.message; return false; }
    *this = std::move(updated);
    message = "판매 완료: +" + std::to_string(proceeds) + " RUB";
    return true;
}

bool Hideout::ConsolidateEquipmentStorage()
{
    auto nextStorage = storage;
    for (int room=0; room<HideoutRoomCount; ++room)
        for (auto kind : {HideoutFurniture::Armory, HideoutFurniture::Wardrobe})
            for (auto item : FurnitureStorageAt(room,kind).Items()) {
                item.slot=-100;
                while (!nextStorage.TryAdd(item))
                    if (!nextStorage.SetSlotCapacity(nextStorage.SlotCapacity()+10)) return false;
            }
    storage=std::move(nextStorage);
    for (int room=0; room<HideoutRoomCount; ++room)
        for (auto kind : {HideoutFurniture::Armory, HideoutFurniture::Wardrobe}) {
            auto& retired=FurnitureStorageAt(room,kind);
            retired=Inventory{};
            retired.SetSlotCapacity(80);
        }
    return true;
}

bool Hideout::DropToFloor(const InventoryCommands::Command& command, const glm::vec3& position, const CollisionWorld& world)
{
    if (!ready || command.action!=InventoryCommands::Action::Move) return false;
    auto next=*this;
    std::vector<ItemStack> dropped;
    if (command.item.source==2 && command.item.id==static_cast<StackId>(ClothingSlot(ItemType::Backpack)+1)) {
        if (!next.loadout.DetachBackpack(next.clothingMask,dropped)) return false;
    } else if (command.item.source==0) {
        const auto* item=next.loadout.Find(command.item.id);
        if (!item) return false;
        dropped.push_back(*item);
        next.loadout.Remove(item->id,item->quantity);
    } else return false;
    for (const auto& item:dropped)
        if (!next.floorLoot.DropNear(position,item,world)) {
            message="아이템을 내려놓을 바닥 공간이 없습니다.";
            return false;
        }
    if (!next.Save()) { message=next.message; return false; }
    *this=std::move(next);
    message="아이템을 바닥에 내려놓았습니다. 바라보고 F로 회수하세요.";
    return true;
}

bool Hideout::TakeFromFloor(const InventoryCommands::Command& command, Player& player, const CollisionWorld& world)
{
    if (!ready || command.item.source!=3) return false;
    auto next=*this;
    auto candidate=player;
    candidate.GetInventory()=loadout;
    candidate.clothingMask=clothingMask;
    candidate.appearance=appearance;
    candidate.vitals=vitals;
    bool taken=false;
    if (command.action==InventoryCommands::Action::Move && command.destination==0)
        taken=next.floorLoot.TakeNearby(command.item.id,candidate,world,command.slot);
    else if (command.action==InventoryCommands::Action::Use)
        taken=next.floorLoot.EquipNearby(command.item.id,candidate,world);
    else if (command.action==InventoryCommands::Action::Move && command.destination==2)
        taken=next.floorLoot.EquipNearby(command.item.id,candidate,world,command.slot);
    if (!taken) { message="줍기/착용 불가: 거리, 휴대 공간과 장비 종류를 확인하세요."; return false; }
    next.loadout=candidate.GetInventory();
    next.clothingMask=candidate.clothingMask;
    if (!next.Save()) { message=next.message; return false; }
    *this=std::move(next);
    player.GetInventory()=loadout;
    player.clothingMask=clothingMask;
    message="바닥 아이템을 회수했습니다.";
    return true;
}

bool Hideout::Save(bool requireStashSpace)
{
    if (!ready) { message = "프로필을 사용할 수 없습니다. 저장 파일을 복구한 뒤 다시 실행하세요."; return false; }
    if (!ConsolidateEquipmentStorage()) { message="기존 장비를 케이스 창고로 옮길 수 없습니다."; return false; }
    RefreshWeapons();
    (void)requireStashSpace;
    std::error_code error;
    std::filesystem::create_directories(RuntimePaths::SaveRoot(), error);
    if (error) { message = "저장 폴더를 만들 수 없습니다. 변경 사항이 저장되지 않았습니다."; return false; }
    std::ofstream out(RuntimePaths::Save("hideout.tmp"), std::ios::trunc);
    out << "HIDEOUT 20\n" << static_cast<int>(appearance) << ' ' << clothingMask << ' '
        << rubles << ' ' << rescuedSurvivors << ' ' << rescuedRoomMask << '\n';
    auto writeInventory = [&](const Inventory& inventory) {
        out << inventory.SlotCapacity() << ' ' << inventory.Items().size() << '\n';
        for (const auto& item : inventory.Items())
            out << static_cast<int>(item.type) << ' ' << item.quantity << ' ' << item.slot << ' '
                << item.rounds << ' ' << item.magazineInserted << ' ' << static_cast<int>(item.fireMode) << '\n';
    };
    writeInventory(storage);
    writeInventory(loadout);
    for (const auto& furniture : furnitureStorage) writeInventory(furniture);
    for (float value : vitals.BodyHealth()) out << value << ' ';
    out << vitals.Hydration() << ' ' << vitals.Stamina() << '\n';
    out << floorLoot.WorldItems().size() << '\n';
    for (const auto& dropped:floorLoot.WorldItems()) {
        const auto& item=dropped.stack;
        out << static_cast<int>(item.type) << ' ' << item.quantity << ' ' << item.rounds << ' '
            << item.magazineInserted << ' ' << static_cast<int>(item.fireMode) << ' '
            << dropped.position.x << ' ' << dropped.position.y << ' ' << dropped.position.z << '\n';
    }
    out << "END\n";
    out.flush();
    const bool written = static_cast<bool>(out);
    out.close();
    if (!written || out.fail() || !MoveFileExW(RuntimePaths::Save("hideout.tmp").c_str(), RuntimePaths::Save("hideout.txt").c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        message = "저장 실패. 여유 공간과 파일 권한을 확인하고 종료 전에 다시 시도하세요.";
        return false;
    }
    message = "은신처를 저장했습니다.";
    return true;
}

static bool StoreGarment(Inventory& storage, ItemType type)
{
    while (!storage.TryAdd({type,1}))
        if (!storage.SetSlotCapacity(storage.SlotCapacity()+10)) return false;
    return true;
}

bool Hideout::SeedStarterSupplies(Hideout& hideout)
{
    for (int room=0;room<InitialRoomCount;++room)
        if (!SeedRoomSupplies(hideout,room)) return false;
    return true;
}

bool Hideout::SeedRoomSupplies(Hideout& hideout, int room)
{
    if (room<0 || room>=HideoutRoomCount) return false;
    auto& fridge=hideout.FurnitureStorageAt(room,HideoutFurniture::Refrigerator);
    auto& armory=hideout.FurnitureStorageAt(room,HideoutFurniture::Armory);
    auto& wardrobe=hideout.FurnitureStorageAt(room,HideoutFurniture::Wardrobe);
    auto& medical=hideout.FurnitureStorageAt(room,HideoutFurniture::MedicalTable);
    if (!fridge.TryAdd({ItemType::Water,4}) ||
        !armory.TryAdd({ItemType::Rifle,1,0,30,true,-100}) ||
        !armory.TryAdd({ItemType::Pistol,1,0,15,true,-100}) ||
        !armory.TryAdd({ItemType::Magazine,2,0,30,false,-100}) ||
        !armory.TryAdd({ItemType::PistolMagazine,2,0,15,false,-100}) ||
        !armory.TryAdd({ItemType::Ammo,60}) ||
        !fridge.TryAdd({ItemType::Food,4}) ||
        !medical.TryAdd({ItemType::Bandage,5}))
        return false;
    for (auto type : {ItemType::Shirt,ItemType::Trousers,ItemType::UnderShirt,
        ItemType::Underpants,ItemType::Trunks,ItemType::Vest,ItemType::Helmet,
        ItemType::Backpack,ItemType::Boots})
        if (!wardrobe.TryAdd({type,1})) return false;
    return true;
}
bool Hideout::ChangeAppearance(PlayerAppearance next, int releasedSurvivorRoom)
{
    if (releasedSurvivorRoom >= 0 &&
        (releasedSurvivorRoom < InitialRoomCount ||
         !HasRescuedSurvivor(releasedSurvivorRoom))) {
        message = "해당 방에 내보낼 구조 인원이 없습니다.";
        return false;
    }
    auto updated = *this;
    bool male = appearance == PlayerAppearance::Male;
    if (male != (next == PlayerAppearance::Male)) {
        for (auto type : {ItemType::UnderShirt, male ? ItemType::Trunks : ItemType::Underpants}) {
            if (!(updated.clothingMask & ClothingBit(type))) continue;
            if (!StoreGarment(updated.storage,type)) return false;
            updated.clothingMask &= ~ClothingBit(type);
        }
    }
    updated.appearance = next;
    if (releasedSurvivorRoom >= 0) {
        updated.rescuedRoomMask &= ~(1u << (releasedSurvivorRoom-InitialRoomCount));
        --updated.rescuedSurvivors;
    }
    if (!updated.Save()) { message=updated.message; return false; }
    *this=std::move(updated);
    return true;
}

void Hideout::Load()
{
    std::error_code error;
    const bool exists = std::filesystem::exists(RuntimePaths::Save("hideout.txt"), error);
    if (error) { message = "은신처 저장 폴더를 읽을 수 없습니다."; return; }
    if (!exists) {
        loadout.SetSlotCapacity(InventorySlotsForGear(clothingMask));
        storage.SetSlotCapacity(80);
        for (auto& furniture : furnitureStorage) furniture.SetSlotCapacity(80);
        for (int type=0; type<11; ++type) if (stash[type]) {
            if (static_cast<ItemType>(type)==ItemType::Water) furnitureStorage[0].TryAdd({ItemType::Water,stash[type]});
            else storage.TryAdd({static_cast<ItemType>(type),stash[type]});
        }
        loadout.TryAdd({ItemType::Rifle,1,0,30,true,-1});
        loadout.TryAdd({ItemType::Magazine,1,0,30,false,12});
        loadout.TryAdd({ItemType::Magazine,1,0,0,false,13});
        ready = true;
        if (!SeedStarterSupplies(*this)) {
            ready = false;
            message = "은신처 기본 물자를 보관함에 배치하지 못했습니다.";
            return;
        }
        Save();
        return;
    }
    std::ifstream in(RuntimePaths::Save("hideout.txt"));
    std::string header;
    int savedVersion=0;
    in >> header >> savedVersion;
    if (header=="HIDEOUT" && (savedVersion>=10 && savedVersion<=20)) {
        Hideout next;
        int appearanceId = 0;
        bool valid = savedVersion < 12 || ((in >> appearanceId) && appearanceId >= 0 && appearanceId <= static_cast<int>(PlayerAppearance::Male));
        // Retired appearance ID 1 uses the default character and its compatible equipment. 
        next.appearance = appearanceId == static_cast<int>(PlayerAppearance::Male) ? PlayerAppearance::Male : PlayerAppearance::Female;
        valid=valid && static_cast<bool>(in >> next.clothingMask) && !(next.clothingMask & ~AllClothingMask);
        if (savedVersion>=14)
            valid=valid && static_cast<bool>(in >> next.rubles) && next.rubles>=0;
        if (savedVersion>=18)
            valid=valid && static_cast<bool>(in >> next.rescuedSurvivors) &&
                next.rescuedSurvivors>=0 && next.rescuedSurvivors<=MaxRescuedSurvivors;
        if (savedVersion>=19) {
            valid=valid && static_cast<bool>(in >> next.rescuedRoomMask) &&
                (next.rescuedRoomMask & ~((1u<<MaxRescuedSurvivors)-1u))==0;
            int occupied=0;
            for (int room=0;room<MaxRescuedSurvivors;++room)
                occupied+=(next.rescuedRoomMask>>room)&1u;
            valid=valid && occupied==next.rescuedSurvivors;
        } else {
            if (next.rescuedSurvivors>=0 && next.rescuedSurvivors<=MaxRescuedSurvivors)
                next.rescuedRoomMask=(1u<<next.rescuedSurvivors)-1u;
        }
        auto readInventory=[&](Inventory& inventory,bool isStorage) {
            int capacity=0,count=0;
            if (!(in >> capacity >> count) || count<0 || count>1000000) return false;
            if (isStorage) { if (capacity<80 || !inventory.SetSlotCapacity(capacity)) return false; }
            else if (capacity!=InventorySlotsForGear(next.clothingMask) || !inventory.ConfigureGear(next.clothingMask)) return false;
            for (int i=0;i<count;++i) {
                int type=-1,inserted=0,mode=0;
                ItemStack item{ItemType::Bandage,0};
                if (!(in >> type >> item.quantity >> item.slot >> item.rounds >> inserted >> mode) ||
                    type<0 || type>=ItemTypeCount || (savedVersion<17 && type>=static_cast<int>(ItemType::Food)) ||
                    mode<0 || mode>2 || inserted<0 || inserted>1) return false;
                item.type=static_cast<ItemType>(type); item.magazineInserted=inserted!=0; item.fireMode=static_cast<FireMode>(mode);
                if (item.quantity<1 || item.quantity>GetItemDefinition(item.type).maxStack ||
                    item.rounds<0 || item.rounds>MagazineCapacity(item.type) ||
                    (!IsWeapon(item.type) && item.magazineInserted) ||
                    (!IsWeapon(item.type) && !IsMagazine(item.type) && item.rounds) ||
                    (IsWeapon(item.type) && !item.magazineInserted && item.rounds) ||
                    item.slot == -100) return false;
                if (savedVersion>=11) {
                    if (inventory.Covering(item.slot) || !inventory.TryAdd(item)) return false;
                } else {
                    if (inventory.TryAdd(item)) continue;
                    item.slot=-100;
                    if (inventory.TryAdd(item)) continue;
                    auto& overflow=next.storage;
                    while (!overflow.TryAdd(item)) {
                        if (!overflow.SetSlotCapacity(overflow.SlotCapacity()+10)) return false;
                    }
                }
            }
            return true;
        };
        valid=valid && readInventory(next.storage,true) && readInventory(next.loadout,false);
        if (savedVersion >= 16) {
            const int savedFurnitureCount=savedVersion>=18
                ? static_cast<int>(furnitureStorage.size()) : InitialRoomCount*FurnitureCount;
            for (int i=0; valid && i<savedFurnitureCount; ++i) {
                auto& furniture=next.furnitureStorage[i];
                valid=readInventory(furniture,true);
                const auto kind=static_cast<HideoutFurniture>(i%FurnitureCount);
                for (const auto& item : furniture.Items())
                    valid=valid && MatchesStorageFilter(FilterFor(kind),item.type);
            }
            for (int i=savedFurnitureCount;valid && i<static_cast<int>(furnitureStorage.size());++i)
                valid=static_cast<bool>(next.furnitureStorage[i].SetSlotCapacity(80));
        } else {
            for (auto& furniture : next.furnitureStorage)
                valid=valid && static_cast<bool>(furniture.SetSlotCapacity(80));
        }
        if (savedVersion == 15) {
            int capacity=0,count=0;
            valid=valid && static_cast<bool>(in >> capacity >> count) && capacity==BaseInventorySlots &&
                count>=0 && count<=BaseInventorySlots &&
                static_cast<bool>(next.furnitureStorage[0].SetSlotCapacity(80));
            for (int i=0; valid && i<count; ++i) {
                int type=-1,inserted=0,mode=0;
                ItemStack item{ItemType::Bandage,0};
                valid=static_cast<bool>(in >> type >> item.quantity >> item.slot >> item.rounds >> inserted >> mode) &&
                    type==static_cast<int>(ItemType::Water) && inserted==0 && mode>=0 && mode<=2 &&
                    item.quantity>0 && item.quantity<=GetItemDefinition(ItemType::Water).maxStack &&
                    item.rounds==0 && item.slot>=0 && item.slot<BaseInventorySlots;
                if (valid) {
                    item.type=ItemType::Water;
                    item.magazineInserted=false;
                    item.fireMode=static_cast<FireMode>(mode);
                    valid=!next.furnitureStorage[0].Covering(item.slot) && next.furnitureStorage[0].TryAdd(item);
                }
            }
        } else if (savedVersion < 16) {
            for (auto& furniture : next.furnitureStorage)
                valid=valid && static_cast<bool>(furniture.SetSlotCapacity(80));
        }
        std::array<float,BodyPartCount> body{};
        float hydration=0,stamina=0;
        for (auto& value:body) valid=static_cast<bool>(in >> value) && valid;
        valid=static_cast<bool>(in >> hydration >> stamina) && valid;
        if (valid && savedVersion>=20) {
            int count=0;
            valid=static_cast<bool>(in >> count) && count>=0 && count<=1000000;
            for (int i=0;valid && i<count;++i) {
                int type=-1,inserted=0,mode=0;
                ItemStack item{ItemType::Bandage,0};
                glm::vec3 position{};
                valid=static_cast<bool>(in >> type >> item.quantity >> item.rounds >> inserted >> mode
                    >> position.x >> position.y >> position.z) && type>=0 && type<ItemTypeCount &&
                    inserted>=0 && inserted<=1 && mode>=0 && mode<=2 &&
                    std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
                if (!valid) break;
                item.type=static_cast<ItemType>(type);
                item.magazineInserted=inserted!=0;
                item.fireMode=static_cast<FireMode>(mode);
                Inventory check;
                check.SetSlotCapacity(80);
                valid=item.quantity>0 && item.quantity<=GetItemDefinition(item.type).maxStack && check.TryAdd(item);
                if (valid) next.floorLoot.Spawn(position,item);
            }
        }
        std::string end,extra;
        valid=valid && next.vitals.Restore(body,hydration,stamina) && static_cast<bool>(in >> end) && end=="END";
        if (in >> extra) valid=false;
        if (!valid) { ready=false; message="저장 파일이 손상되었습니다. 원본을 보존했습니다."; return; }
        next.ready=true; next.RefreshWeapons(); next.message="은신처를 불러왔습니다.";
        if (savedVersion<13) {
            if (!StoreGarment(next.storage,ItemType::Trunks)) return;
            if (next.appearance==PlayerAppearance::Male && (next.clothingMask & ClothingBit(ItemType::UnderShirt))) {
                if (!StoreGarment(next.storage,ItemType::UnderShirt)) return;
                next.clothingMask &= ~ClothingBit(ItemType::UnderShirt);
            }
        }
        if (savedVersion<15) {
            const auto priorStorage=next.storage.Items();
            for (auto item : priorStorage) {
                if (item.type==ItemType::Water) item.slot=-100;
                if (item.type==ItemType::Water && next.furnitureStorage[0].TryAdd(item))
                    next.storage.Remove(item.id,item.quantity);
            }
        }
        if (savedVersion < 17 && !SeedStarterSupplies(next)) {
            ready=false;
            message="은신처 기본 물자를 보관함에 배치하지 못했습니다. 저장 파일은 보존했습니다.";
            return;
        }
        in.close();
        if (savedVersion < 20 && !next.Save()) {
            message=next.message;
            return;
        }
        *this=std::move(next); return;
    }
    in.clear(); in.seekg(0);
    Hideout next;
    next.stash.fill(0);
    std::string tag, end;
    int version = 0, equipped = 0, count = 0;
    bool valid = static_cast<bool>(in >> tag >> version >> next.rifles >> equipped >> next.magazine);
    valid = valid && tag == "HIDEOUT" && (version >= 1 && version <= 9) && next.rifles >= 0 && next.rifles <= MaxStored &&
        (equipped == 0 || equipped == 1) && next.magazine >= 0 && next.magazine <= 30 && (equipped || next.magazine == 0);
    const int stockCount = version == 1 ? 3 : version < 4 ? 7 : version < 5 ? 9 : version < 9 ? 10 : 11;
    for (int i = 0; i < stockCount; ++i) {
        auto& stock = next.stash[i];
        valid = static_cast<bool>(in >> stock) && valid;
        valid = valid && stock >= 0 && stock <= MaxStored;
    }
    if (version >= 8) {
        int placementCount = 0;
        valid = static_cast<bool>(in >> placementCount) && valid;
        valid = valid && placementCount >= 0 && placementCount <= StashColumns * StashRows;
        for (int i = 0; valid && i < placementCount; ++i) {
            StashPlacement placement{};
            valid = static_cast<bool>(in >> placement.type >> placement.x >> placement.y) &&
                placement.type >= 0 && placement.type <= stockCount;
            if (valid) {
                if (placement.type == stockCount) placement.type = ItemTypeCount; // Old rifle sentinel.
                if (placement.x < 0 || placement.x >= StashColumns || placement.y < 0 || placement.y >= StashRows) valid = false;
            }
        }
    }
    if (version >= 2) {
        int clothing = -1;
        valid = static_cast<bool>(in >> clothing) && valid;
        const int allowedMask = version < 4 ? 15 : version < 5 ? 63 : version < 9 ? 127 : static_cast<int>(AllClothingMask);
        valid = valid && clothing >= 0 && clothing <= allowedMask;
        if (valid) next.clothingMask = static_cast<std::uint32_t>(clothing);
    }
    if (version == 2) {
        // v2 footwear is now permanently worn; its IDs must not become underwear.
        next.clothingMask = (next.clothingMask & 3u) | 12u;
    }
    if (version < 4) next.clothingMask &= 15u;
    if (version < 5) {
        next.clothingMask &= (1u << 6) - 1;
    }
    if (valid) next.loadout.SetSlotCapacity(InventorySlotsForGear(next.clothingMask));
    valid = static_cast<bool>(in >> count) && valid;
    valid = valid && count >= 0 && count <= next.loadout.SlotCapacity();
    for (int i = 0; valid && i < count; ++i) {
        int type = -1, quantity = 0;
        valid = static_cast<bool>(in >> type >> quantity) && type >= 0 && type < stockCount && quantity > 0;
        if (valid) {
            valid = quantity <= (type == static_cast<int>(ItemType::Ammo) ? 60 : GetItemDefinition(static_cast<ItemType>(type)).maxStack);
            if (valid && !(version == 2 && type >= 5))
                if (!next.loadout.TryAdd({static_cast<ItemType>(type), quantity})) {
                    if (next.stash[type] > MaxStored - quantity) valid=false;
                    else next.stash[type] += quantity;
                }
        }
    }
    if (version >= 7) {
        std::array<float, BodyPartCount> body{};
        float hydration = 0, stamina = 0;
        for (auto& value : body) valid = static_cast<bool>(in >> value) && valid;
        valid = static_cast<bool>(in >> hydration >> stamina) && valid;
        if (valid) valid = next.vitals.Restore(body, hydration, stamina);
    }
    valid = static_cast<bool>(in >> end) && end == "END" && valid;
    std::string extra;
    if (in >> extra) valid = false;
    if (!valid) { ready = false; message = "은신처 저장 파일이 손상되었습니다. 원본은 보존했으며, 플레이 전에 복구해야 합니다."; return; }
    next.rifle = equipped != 0;
    // Preserve legacy overflow stock instead of silently discarding it.
    next.storage.SetSlotCapacity(80);
    for (auto& furniture : next.furnitureStorage) furniture.SetSlotCapacity(80);
    auto preserve=[&](ItemStack item) {
        while (!next.storage.TryAdd(item))
            if (!next.storage.SetSlotCapacity(next.storage.SlotCapacity()+10)) return false;
        return true;
    };
    for (int type=0;type<11;++type) if (next.stash[type] && !preserve({static_cast<ItemType>(type),next.stash[type]})) {
        ready=false; message="기존 창고를 재배치할 수 없습니다. 원본을 보존했습니다."; return;
    }
    const auto priorStorage=next.storage.Items();
    for (auto item : priorStorage) {
        if (item.type==ItemType::Water) item.slot=-100;
        if (item.type==ItemType::Water && next.furnitureStorage[0].TryAdd(item))
            next.storage.Remove(item.id,item.quantity);
    }
    if (!preserve({ItemType::Trunks,1})) return;
    if (next.rifles && !preserve({ItemType::Rifle,next.rifles})) {
        ready=false; message="기존 총기를 재배치할 수 없습니다. 원본을 보존했습니다."; return;
    }
    if (next.rifle) next.loadout.TryAdd({ItemType::Rifle, 1, 0, next.magazine, true, -1});
    next.ready = true;
    next.message = version < 9 ? "저장 데이터를 새 형식으로 변환했습니다." : "은신처를 불러왔습니다.";
    if (!SeedStarterSupplies(next)) {
        ready=false; message="은신처 기본 물자를 보관함에 배치하지 못했습니다. 저장 파일은 보존했습니다."; return;
    }
    *this = std::move(next);
    in.close();
    Save();
}

void Hideout::RefreshWeapons()
{
    rifle = loadout.At(-1) || loadout.At(-2);
    const auto* weapon = loadout.At(-1) ? loadout.At(-1) : loadout.At(-2);
    magazine = weapon ? weapon->rounds : 0;
}

bool Hideout::DrinkWater(StackId id, bool fromStash)
{
    if (!ready) return false;
    auto before = *this;
    Inventory* source = &loadout;
    if (fromStash) {
        if (id) {
            bool found=false;
            for (int room=0; room<HideoutRoomCount; ++room) {
                auto& fridge=FurnitureStorageAt(room,HideoutFurniture::Refrigerator);
                if (fridge.Find(id)) { source=&fridge; found=true; break; }
            }
            if (!found && storage.Find(id)) source=&storage;
        }
        else if (FurnitureStorageAt(0,HideoutFurniture::Refrigerator).Count(ItemType::Water))
            source=&FurnitureStorageAt(0,HideoutFurniture::Refrigerator);
        else if (FurnitureStorageAt(1,HideoutFurniture::Refrigerator).Count(ItemType::Water))
            source=&FurnitureStorageAt(1,HideoutFurniture::Refrigerator);
        else if (!id && storage.Count(ItemType::Water)) source = &storage;
    }
    if (!id) for (const auto& entry : source->Items()) if (entry.type == ItemType::Water) { id = entry.id; break; }
    const auto* item = source->Find(id);
    if (!item || item->type != ItemType::Water || !vitals.Drink()) { message = "물이 없거나 수분이 가득 찼습니다."; return false; }
    source->Remove(id, 1);
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; return false; }
    return true;
}

bool Hideout::HealBodyPart(BodyPart part, bool fromStash)
{
    if (!ready) return false;
    auto before = *this;
    Inventory& source = fromStash && storage.Count(ItemType::Bandage) ? storage : loadout;
    if (!source.Count(ItemType::Bandage) || !vitals.Heal(part)) { message = "붕대가 없거나 치료할 필요가 없습니다."; return false; }
    source.Take(ItemType::Bandage, 1);
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; return false; }
    return true;
}

bool Hideout::Depart()
{
    if (!ready) return false;
    // Stage the complete departure before touching either the live profile or disk.
    Hideout departing = *this;
    if (appearance == PlayerAppearance::Female) {
        for (auto item : loadout.Items()) {
            item.id = 0;
            item.slot = -100;
            if (!departing.storage.TryAdd(item)) {
                message = "SCAV 출격 취소: 개인 장비를 보관할 창고 공간이 부족합니다. 창고를 정리하세요.";
                return false;
            }
        }
        // Worn clothes are represented by bits, not loadout stacks.
        for (int slot = 0; slot < ClothingSlotCount; ++slot) {
            if (!(clothingMask & (1u << slot))) continue;
            if (!departing.storage.TryAdd({ClothingType(slot), 1})) {
                message = "SCAV 출격 취소: 착용 의류를 보관할 창고 공간이 부족합니다. 창고를 정리하세요.";
                return false;
            }
        }
        departing.loadout = Inventory{};
        departing.clothingMask = ClothingBit(ItemType::Shirt) | ClothingBit(ItemType::Trousers)
            | ClothingBit(ItemType::UnderShirt) | ClothingBit(ItemType::Underpants)
            | ClothingBit(ItemType::Boots) | ClothingBit(ItemType::Vest);
        static std::mt19937 random(std::random_device{}());
        const int kit = std::uniform_int_distribution<int>(0, 2)(random);
        if (kit == 1) departing.clothingMask |= ClothingBit(ItemType::Helmet);
        if (kit == 2) departing.clothingMask |= ClothingBit(ItemType::Backpack);
        const int rounds = std::uniform_int_distribution<int>(10, 30)(random);
        if (!departing.loadout.ConfigureGear(departing.clothingMask) ||
            !departing.loadout.TryAdd({ItemType::Rifle, 1, 0, rounds, true, -1}) ||
            !departing.loadout.TryAdd({ItemType::Magazine, 1, 0, 30, false, 12}) ||
            !departing.loadout.TryAdd({ItemType::Bandage, kit + 1}) ||
            !departing.loadout.TryAdd({ItemType::Water, 1})) {
            message = "SCAV 지급 장비를 구성하지 못해 출격을 취소했습니다.";
            return false;
        }
        departing.RefreshWeapons();
    }
    // Persist only safe stash contents. Raid gear is recovered on extraction;
    // closing the game during a raid must not duplicate the issued kit.
    Hideout away = departing;
    away.loadout = Inventory{};
    away.rifle = false;
    away.magazine = 0;
    away.clothingMask = 0;
    if (!away.Save()) { message = away.message; return false; }
    *this = std::move(departing);
    return true;
}
bool Hideout::Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing,
    bool rescuedSurvivor)
{
    (void)weapon; (void)rounds;
    auto updated=*this;
    updated.loadout=items;
    updated.clothingMask=clothing&AllClothingMask;
    updated.loadout.ConfigureGear(updated.clothingMask);
    if (rescuedSurvivor && updated.rescuedSurvivors<MaxRescuedSurvivors) {
        int room=InitialRoomCount;
        while (room<HideoutRoomCount && updated.HasRescuedSurvivor(room)) ++room;
        if (room>=HideoutRoomCount) {
            message="새 은신처 방을 찾을 수 없습니다.";
            return false;
        }
        if (!SeedRoomSupplies(updated,room)) {
            message="새 은신처 방의 기본 물자를 배치하지 못했습니다.";
            return false;
        }
        updated.rescuedRoomMask|=1u<<(room-InitialRoomCount);
        ++updated.rescuedSurvivors;
    }
    if (!updated.Save()) {
        const auto failure=updated.message;
        *this=std::move(updated);
        message=failure;
        return false;
    }
    *this=std::move(updated);
    return true;
}
