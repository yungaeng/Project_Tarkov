#include "ThirdPersonCameraController.h"
#include "Camera.h"
#include "../Game/Character.h"
#include <algorithm>

void ThirdPersonCameraController::Rotate(Camera& camera, const glm::vec2& mouseDelta) const
{
    camera.yaw += mouseDelta.x * settings.sensitivity;
    camera.pitch = std::clamp(camera.pitch + mouseDelta.y * settings.sensitivity,
        -settings.pitchLimit, settings.pitchLimit);
    camera.UpdateDirection();
}

void ThirdPersonCameraController::Follow(Camera& camera, const Character& character) const
{
    const float eyeHeight = character.GetEyeHeight();
    camera.position = character.position - camera.front * settings.distance
        + glm::vec3(0, eyeHeight + settings.heightOffset, 0);
}
