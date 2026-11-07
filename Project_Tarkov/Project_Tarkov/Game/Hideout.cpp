#include "Hideout.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace { constexpr int MaxStored = 1000000; }

bool Hideout::AutoArrangeStash()
{
    if (!ready) return false;
    auto before=*this;
    storage.AutoArrange();
    if (!Save()) { const auto failure=message; *this=std::move(before); message=failure; return false; }
    return true;
}

bool Hideout::Save(bool requireStashSpace)
{
    if (!ready) { message = "프로필을 사용할 수 없습니다. 저장 파일을 복구한 뒤 다시 실행하세요."; return false; }
    RefreshWeapons();
    (void)requireStashSpace;
    std::error_code error;
    std::filesystem::create_directories("Saves", error);
    if (error) { message = "저장 폴더를 만들 수 없습니다. 변경 사항이 저장되지 않았습니다."; return false; }
    std::ofstream out("Saves/hideout.tmp", std::ios::trunc);
    out << "HIDEOUT 13\n" << static_cast<int>(appearance) << ' ' << clothingMask << '\n';
    auto writeInventory = [&](const Inventory& inventory) {
        out << inventory.SlotCapacity() << ' ' << inventory.Items().size() << '\n';
        for (const auto& item : inventory.Items())
            out << static_cast<int>(item.type) << ' ' << item.quantity << ' ' << item.slot << ' '
                << item.rounds << ' ' << item.magazineInserted << ' ' << static_cast<int>(item.fireMode) << '\n';
    };
    writeInventory(storage);
    writeInventory(loadout);
    for (float value : vitals.BodyHealth()) out << value << ' ';
    out << vitals.Hydration() << ' ' << vitals.Stamina() << '\n';
    out << "END\n";
    out.flush();
    const bool written = static_cast<bool>(out);
    out.close();
    if (!written || out.fail() || !MoveFileExW(L"Saves/hideout.tmp", L"Saves/hideout.txt",
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

bool Hideout::ChangeAppearance(PlayerAppearance next)
{
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
    if (!updated.Save()) { message=updated.message; return false; }
    *this=std::move(updated);
    return true;
}

void Hideout::Load()
{
    std::error_code error;
    const bool exists = std::filesystem::exists("Saves/hideout.txt", error);
    if (error) { message = "은신처 저장 폴더를 읽을 수 없습니다."; return; }
    if (!exists) {
        loadout.SetSlotCapacity(InventorySlotsForGear(clothingMask));
        storage.SetSlotCapacity(80);
        for (int type=0; type<11; ++type) if (stash[type]) storage.TryAdd({static_cast<ItemType>(type),stash[type]});
        storage.TryAdd({ItemType::Trunks,1});
        loadout.TryAdd({ItemType::Rifle,1,0,30,true,-1});
        loadout.TryAdd({ItemType::Magazine,1,0,30,false,12});
        loadout.TryAdd({ItemType::Magazine,1,0,0,false,13});
        ready = true;
        Save();
        return;
    }
    std::ifstream in("Saves/hideout.txt");
    std::string header;
    int savedVersion=0;
    in >> header >> savedVersion;
    if (header=="HIDEOUT" && (savedVersion>=10 && savedVersion<=13)) {
        Hideout next;
        int appearanceId = 0;
        bool valid = savedVersion < 12 || ((in >> appearanceId) && appearanceId >= 0 && appearanceId <= static_cast<int>(PlayerAppearance::Male));
        // Retired appearance ID 1 uses the default character and its compatible equipment. 
        next.appearance = appearanceId == static_cast<int>(PlayerAppearance::Male) ? PlayerAppearance::Male : PlayerAppearance::Female;
        valid=valid && static_cast<bool>(in >> next.clothingMask) && !(next.clothingMask & ~AllClothingMask);
        auto readInventory=[&](Inventory& inventory,bool isStorage) {
            int capacity=0,count=0;
            if (!(in >> capacity >> count) || count<0 || count>1000000) return false;
            if (isStorage) { if (capacity<80 || !inventory.SetSlotCapacity(capacity)) return false; }
            else if (capacity!=InventorySlotsForGear(next.clothingMask) || !inventory.ConfigureGear(next.clothingMask)) return false;
            for (int i=0;i<count;++i) {
                int type=-1,inserted=0,mode=0;
                ItemStack item{ItemType::Bandage,0};
                if (!(in >> type >> item.quantity >> item.slot >> item.rounds >> inserted >> mode) ||
                    type<0 || type>=ItemTypeCount || mode<0 || mode>2 || inserted<0 || inserted>1) return false;
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
        std::array<float,BodyPartCount> body{};
        float hydration=0,stamina=0;
        for (auto& value:body) valid=static_cast<bool>(in >> value) && valid;
        valid=static_cast<bool>(in >> hydration >> stamina) && valid;
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
            in.close();
            if (!next.Save()) { message=next.message; return; }
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
    auto preserve=[&](ItemStack item) {
        while (!next.storage.TryAdd(item))
            if (!next.storage.SetSlotCapacity(next.storage.SlotCapacity()+10)) return false;
        return true;
    };
    for (int type=0;type<11;++type) if (next.stash[type] && !preserve({static_cast<ItemType>(type),next.stash[type]})) {
        ready=false; message="기존 창고를 재배치할 수 없습니다. 원본을 보존했습니다."; return;
    }
    if (!preserve({ItemType::Trunks,1})) return;
    if (next.rifles && !preserve({ItemType::Rifle,next.rifles})) {
        ready=false; message="기존 총기를 재배치할 수 없습니다. 원본을 보존했습니다."; return;
    }
    if (next.rifle) next.loadout.TryAdd({ItemType::Rifle, 1, 0, next.magazine, true, -1});
    next.ready = true;
    next.message = version < 9 ? "저장 데이터를 새 형식으로 변환했습니다." : "은신처를 불러왔습니다.";
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
    auto& source = fromStash && (id || storage.Count(ItemType::Water)) ? storage : loadout;
    if (!id) for (const auto& item : source.Items()) if (item.type == ItemType::Water) { id = item.id; break; }
    const auto* item = source.Find(id);
    if (!item || item->type != ItemType::Water || !vitals.Drink()) { message = "물이 없거나 수분이 가득 찼습니다."; return false; }
    source.Remove(id, 1);
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; return false; }
    return true;
}

bool Hideout::HealBodyPart(BodyPart part)
{
    if (!ready) return false;
    auto before = *this;
    Inventory& source = storage.Count(ItemType::Bandage) ? storage : loadout;
    if (!source.Count(ItemType::Bandage) || !vitals.Heal(part)) { message = "붕대가 없거나 치료할 필요가 없습니다."; return false; }
    source.Take(ItemType::Bandage, 1);
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; return false; }
    return true;
}

bool Hideout::Depart()
{
    Hideout away = *this;
    away.loadout = Inventory{}; away.rifle = false; away.magazine = 0; away.clothingMask = 0;
    if (!away.Save()) { message = away.message; return false; }
    return true;
}

void Hideout::Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing)
{
    (void)weapon; (void)rounds;
    loadout = items;
    clothingMask = clothing & AllClothingMask;
    loadout.ConfigureGear(clothingMask);
    RefreshWeapons();
    Save();
}
