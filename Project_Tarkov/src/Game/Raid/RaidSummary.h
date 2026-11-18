#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../Character/PlayerAppearance.h"
#include <glm/vec3.hpp>

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
    PlayerAppearance rescuedAppearance = PlayerAppearance::Female;
    std::uint32_t rescuedClothing = 0;
    glm::vec3 rescuedScale{1.f};
    std::vector<std::string> acquiredEquipment;
    std::vector<std::string> lostEquipment;
    std::vector<KillRecord> kills;
};
