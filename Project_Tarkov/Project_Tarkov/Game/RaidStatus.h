#pragma once
#include "Config/RaidConfig.h"
#include "Inventory.h"
#include <string>

enum class RaidPhase { Hideout, Active, Extracted, Dead, Missing };
struct RaidStatus
{
    RaidPhase phase = RaidPhase::Hideout;
    float remaining = RaidConfig::RaidDuration;
    float extraction = 0;
    int assignedExit = 0;
    float exitDistance = 0;
    Inventory recovered;
    std::string saveMessage;
    bool returnToHideout = false;
    bool Finished() const { return phase == RaidPhase::Extracted || phase == RaidPhase::Dead || phase == RaidPhase::Missing; }
};
