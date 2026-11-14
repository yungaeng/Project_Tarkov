#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct KillRecord
{
    std::string target;
    std::string time;
    std::string location;
    std::string hitLocation;
};

struct RaidSummary
{
    enum class Outcome { Extracted, Dead, Missing };
    Outcome outcome = Outcome::Extracted;
    int durationSeconds = 0;
    std::uint32_t recoveredClothing = 0;
    bool recoveredRifle = false;
    bool rescuedFemale = false;
    std::vector<std::string> acquiredEquipment;
    std::vector<std::string> lostEquipment;
    std::vector<KillRecord> kills;
};
