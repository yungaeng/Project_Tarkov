#include "Hideout.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace { constexpr int MaxStored = 1000000; }

bool Hideout::Save()
{
    if (!ready) { message = "프로필을 사용할 수 없습니다. 저장 파일을 복구한 뒤 다시 실행하세요."; return false; }
    std::error_code error;
    std::filesystem::create_directories("Saves", error);
    if (error) { message = "저장 폴더를 만들 수 없습니다. 변경 사항이 저장되지 않았습니다."; return false; }
    std::ofstream out("Saves/hideout.tmp", std::ios::trunc);
    out << "HIDEOUT 6\n" << rifles << ' ' << rifle << ' ' << magazine << '\n';
    for (int count : stash) out << count << ' ';
    out << '\n' << clothingMask << '\n' << loadout.Items().size() << '\n';
    for (const auto& item : loadout.Items()) out << static_cast<int>(item.type) << ' ' << item.quantity << '\n';
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

void Hideout::Load()
{
    std::error_code error;
    const bool exists = std::filesystem::exists("Saves/hideout.txt", error);
    if (error) { message = "은신처 저장 폴더를 읽을 수 없습니다."; return; }
    if (!exists) {
        loadout.SetSlotCapacity(InventorySlotsForGear(clothingMask));
        ready = true;
        Save();
        return;
    }
    std::ifstream in("Saves/hideout.txt");
    Hideout next;
    next.stash.fill(0);
    std::string tag, end;
    int version = 0, equipped = 0, count = 0;
    bool valid = static_cast<bool>(in >> tag >> version >> next.rifles >> equipped >> next.magazine);
    valid = valid && tag == "HIDEOUT" && (version >= 1 && version <= 6) && next.rifles >= 0 && next.rifles <= MaxStored &&
        (equipped == 0 || equipped == 1) && next.magazine >= 0 && next.magazine <= 30 && (equipped || next.magazine == 0);
    const int stockCount = version == 1 ? 3 : version < 4 ? 7 : version < 5 ? 9 : ItemTypeCount;
    for (int i = 0; i < stockCount; ++i) {
        auto& stock = next.stash[i];
        valid = static_cast<bool>(in >> stock) && valid;
        valid = valid && stock >= 0 && stock <= MaxStored;
    }
    if (version >= 2) {
        int clothing = -1;
        valid = static_cast<bool>(in >> clothing) && valid;
        const int allowedMask = version < 4 ? 15 : version < 5 ? 63 : static_cast<int>(AllClothingMask);
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
    // One-time test grant for existing profiles; preserve their saved inventory.
    if (version < 6) {
        next.rifles = (std::min)(MaxStored, next.rifles + 50);
        for (auto& stock : next.stash) stock = (std::min)(MaxStored, stock + 50);
    }
    if (valid) next.loadout.SetSlotCapacity(InventorySlotsForGear(next.clothingMask));
    valid = static_cast<bool>(in >> count) && valid;
    valid = valid && count >= 0 && count <= next.loadout.SlotCapacity();
    for (int i = 0; valid && i < count; ++i) {
        int type = -1, quantity = 0;
        valid = static_cast<bool>(in >> type >> quantity) && type >= 0 && type < stockCount && quantity > 0;
        if (valid) {
            valid = quantity <= GetItemDefinition(static_cast<ItemType>(type)).maxStack;
            if (valid && !(version == 2 && type >= 5))
                valid = next.loadout.TryAdd({static_cast<ItemType>(type), quantity});
        }
    }
    valid = static_cast<bool>(in >> end) && end == "END" && valid;
    std::string extra;
    if (in >> extra) valid = false;
    if (!valid) { ready = false; message = "은신처 저장 파일이 손상되었습니다. 원본은 보존했으며, 플레이 전에 복구해야 합니다."; return; }
    next.rifle = equipped != 0;
    next.ready = true;
    next.message = version < 6 ? "테스트 아이템 50개를 지급했습니다." : "은신처를 불러왔습니다.";
    *this = std::move(next);
    if (version < 6) Save();
}

void Hideout::Transfer(ItemType type, int quantity, bool take)
{
    if (!ready || quantity <= 0 || static_cast<int>(type) < 0 || type >= ItemType::Count) return;
    Hideout before = *this;
    auto& stock = stash[static_cast<std::size_t>(type)];
    quantity = (std::min)(quantity, take ? stock : loadout.Count(type));
    if (!quantity) return;
    if (take) {
        if (!loadout.TryAdd({type, quantity})) { message = "출전 장비 공간이 가득 찼습니다."; return; }
        stock -= quantity;
    } else {
        if (stock > MaxStored - quantity) { message = "창고가 가득 찼습니다."; return; }
        stock += loadout.Take(type, quantity);
    }
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; }
}

void Hideout::StoreStack(StackId id)
{
    if (!ready) return;
    const auto* item = loadout.Find(id);
    if (!item) { message = "해당 아이템이 더 이상 없습니다."; return; }
    const auto type = static_cast<std::size_t>(item->type);
    const int quantity = item->quantity;
    if (stash[type] > MaxStored - quantity) { message = "창고가 가득 찼습니다."; return; }
    Hideout before = *this;
    if (!loadout.Remove(id, quantity)) return;
    stash[type] += quantity;
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; }
}

void Hideout::ToggleRifle()
{
    if (!ready) return;
    Hideout before = *this;
    if (rifle) {
        if (rifles >= MaxStored || stash[2] > MaxStored - magazine) { message = "창고가 가득 찼습니다."; return; }
        ++rifles; stash[2] += magazine; magazine = 0; rifle = false;
    } else {
        if (rifles <= 0) { message = "창고에 소총이 없습니다. 비무장 상태로 출전할 수 있습니다."; return; }
        --rifles; rifle = true;
    }
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; }
}

void Hideout::LoadMagazine()
{
    if (!ready || !rifle) return;
    Hideout before = *this;
    const int rounds = (std::min)(30 - magazine, stash[2]);
    magazine += rounds; stash[2] -= rounds;
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; }
}

void Hideout::ToggleClothing(ItemType type, bool fromStash)
{
    if (!ready || !IsClothing(type)) return;
    Hideout before = *this;
    const auto bit = ClothingBit(type);
    auto& stock = stash[static_cast<std::size_t>(type)];
    if (clothingMask & bit) {
        if (stock >= MaxStored) { message = "창고가 가득 찼습니다."; return; }
        const auto nextMask = clothingMask & ~bit;
        if (!loadout.SetSlotCapacity(InventorySlotsForGear(nextMask))) {
            message = "장착 해제 전에 휴대품을 줄여 해당 슬롯을 비우세요.";
            return;
        }
        ++stock;
        clothingMask = nextMask;
    } else {
        // Prefer a carried item; otherwise equip directly from the secure stash.
        if (fromStash) {
            if (stock <= 0) { message = "창고에 해당 장비가 없습니다."; return; }
            --stock;
        } else if (loadout.Count(type) > 0) loadout.Take(type, 1);
        else if (stock > 0) --stock;
        else { message = "착용할 의상이 없습니다."; return; }
        clothingMask |= bit;
        loadout.SetSlotCapacity(InventorySlotsForGear(clothingMask));
    }
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; }
}

bool Hideout::Depart()
{
    // Save only the protected stash: quitting/crashing during a raid forfeits deployed gear.
    Hideout away = *this;
    away.loadout = Inventory{}; away.rifle = false; away.magazine = 0;
    away.clothingMask = 0;
    if (!away.Save()) { message = away.message; return false; }
    return true;
}

void Hideout::Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing)
{
    loadout = items; rifle = weapon; magazine = weapon ? rounds : 0;
    clothingMask = clothing & AllClothingMask;
    loadout.SetSlotCapacity(InventorySlotsForGear(clothingMask));
    Save(); // On failure keep recovered items in memory, and allow retry in the hideout.
}
