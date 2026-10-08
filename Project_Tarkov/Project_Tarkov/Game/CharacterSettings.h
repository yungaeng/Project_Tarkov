#pragma once
#include "Config/RaidConfig.h"

// World distances are meters; imported character models use centimeters.
struct CharacterSettings
{
    float walkSpeed = RaidConfig::WalkSpeed;
    float runSpeed = RaidConfig::RunSpeed;
    float crouchSpeed = 2.5f;
    float bodyRadius = 0.35f;
    float bodyHeight = 1.8f;
    float crouchingBodyHeight = 1.1f;
    float gravity = 9.81f;
    float terminalSpeed = 50.0f;
    float standingEyeHeight = 1.7f;
    float crouchingEyeHeight = 1.0f;
    float modelScale = 0.01f;
};

struct ThirdPersonCameraSettings
{
    float sensitivity = 0.1f;
    float distance = 6.0f;
    float heightOffset = 1.0f;
    float pitchLimit = 89.0f;
};
