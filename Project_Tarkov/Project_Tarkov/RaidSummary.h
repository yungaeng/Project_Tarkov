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
    int durationSeconds = 0;
    std::uint32_t recoveredClothing = 0;
    bool recoveredRifle = false;
    std::vector<std::string> acquiredEquipment;
    std::vector<KillRecord> kills;
};
