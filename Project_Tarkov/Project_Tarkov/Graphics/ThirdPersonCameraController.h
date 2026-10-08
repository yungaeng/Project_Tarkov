#pragma once
#include "../Game/CharacterSettings.h"
#include <glm/vec2.hpp>

class Camera;
class Character;

class ThirdPersonCameraController
{
public:
    void Rotate(Camera& camera, const glm::vec2& mouseDelta) const;
    void Follow(Camera& camera, const Character& character) const;

private:
    ThirdPersonCameraSettings settings;
};
