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
    if (!ready) { message = "Profile unavailable. Fix the save file and restart."; return false; }
    std::error_code error;
    std::filesystem::create_directories("Saves", error);
    if (error) { message = "Cannot create save folder. Changes were not saved."; return false; }
    std::ofstream out("Saves/hideout.tmp", std::ios::trunc);
    out << "HIDEOUT 1\n" << rifles << ' ' << rifle << ' ' << magazine << '\n';
    for (int count : stash) out << count << ' ';
    out << '\n' << loadout.Items().size() << '\n';
    for (const auto& item : loadout.Items()) out << static_cast<int>(item.type) << ' ' << item.quantity << '\n';
    out << "END\n";
    out.flush();
    const bool written = static_cast<bool>(out);
    out.close();
    if (!written || out.fail() || !MoveFileExW(L"Saves/hideout.tmp", L"Saves/hideout.txt",
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        message = "Save failed. Check free space and file permissions; retry before leaving.";
        return false;
    }
    message = "Hideout saved.";
    return true;
}

void Hideout::Load()
{
    std::error_code error;
    const bool exists = std::filesystem::exists("Saves/hideout.txt", error);
    if (error) { message = "Cannot read hideout save folder."; return; }
    if (!exists) { ready = true; Save(); return; }
    std::ifstream in("Saves/hideout.txt");
    Hideout next;
    std::string tag, end;
    int version = 0, equipped = 0, count = 0;
    bool valid = static_cast<bool>(in >> tag >> version >> next.rifles >> equipped >> next.magazine);
    valid = valid && tag == "HIDEOUT" && version == 1 && next.rifles >= 0 && next.rifles <= MaxStored &&
        (equipped == 0 || equipped == 1) && next.magazine >= 0 && next.magazine <= 30 && (equipped || next.magazine == 0);
    for (auto& stock : next.stash) {
        valid = static_cast<bool>(in >> stock) && valid;
        valid = valid && stock >= 0 && stock <= MaxStored;
    }
    valid = static_cast<bool>(in >> count) && valid;
    valid = valid && count >= 0 && count <= static_cast<int>(Inventory::Capacity);
    for (int i = 0; valid && i < count; ++i) {
        int type = -1, quantity = 0;
        valid = static_cast<bool>(in >> type >> quantity) && type >= 0 && type < 3 && quantity > 0;
        if (valid) valid = quantity <= GetItemDefinition(static_cast<ItemType>(type)).maxStack &&
            next.loadout.TryAdd({static_cast<ItemType>(type), quantity});
    }
    valid = static_cast<bool>(in >> end) && end == "END" && valid;
    std::string extra;
    if (in >> extra) valid = false;
    if (!valid) { ready = false; message = "Invalid hideout save. Original file preserved; repair it before playing."; return; }
    next.rifle = equipped != 0;
    next.ready = true;
    next.message = "Hideout loaded.";
    *this = std::move(next);
}

void Hideout::Transfer(ItemType type, int quantity, bool take)
{
    if (!ready || quantity <= 0) return;
    Hideout before = *this;
    auto& stock = stash[static_cast<std::size_t>(type)];
    quantity = (std::min)(quantity, take ? stock : loadout.Count(type));
    if (!quantity) return;
    if (take) {
        if (!loadout.TryAdd({type, quantity})) { message = "Loadout is full."; return; }
        stock -= quantity;
    } else {
        if (stock > MaxStored - quantity) { message = "Stash capacity reached."; return; }
        stock += loadout.Take(type, quantity);
    }
    if (!Save()) { auto failure = message; *this = std::move(before); message = failure; }
}

void Hideout::ToggleRifle()
{
    if (!ready) return;
    Hideout before = *this;
    if (rifle) {
        if (rifles >= MaxStored || stash[2] > MaxStored - magazine) { message = "Stash capacity reached."; return; }
        ++rifles; stash[2] += magazine; magazine = 0; rifle = false;
    } else {
        if (rifles <= 0) { message = "No rifle in stash. You can deploy unarmed."; return; }
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

bool Hideout::Depart()
{
    // Save only the protected stash: quitting/crashing during a raid forfeits deployed gear.
    Hideout away = *this;
    away.loadout = Inventory{}; away.rifle = false; away.magazine = 0;
    if (!away.Save()) { message = away.message; return false; }
    return true;
}

void Hideout::Recover(const Inventory& items, bool weapon, int rounds)
{
    loadout = items; rifle = weapon; magazine = weapon ? rounds : 0;
    Save(); // On failure keep recovered items in memory, and allow retry in the hideout.
}
