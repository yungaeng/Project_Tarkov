#include "PlayerController.h"
#include "Player.h"
#include "../Graphics/Camera.h"
#include "../Core/Input.h"
#include <glm/geometric.hpp>

void PlayerController::UpdateInterface()
{
    if (Input::GetKeyDown(GLFW_KEY_I))
    {
        inventoryOpen = !inventoryOpen;
        Input::SetCursorCaptured(!inventoryOpen);
    }
}

bool PlayerController::IsGameplayInputEnabled() const
{
    return !inventoryOpen && Input::IsFocused();
}

void PlayerController::Reset()
{
    inventoryOpen = false;
    Input::SetCursorCaptured(true);
}

void PlayerController::UpdateMovement(Player& player, const Camera& camera) const
{
    player.StopMovement();
    player.SetSprinting(false);
    if (!IsGameplayInputEnabled() || !player.vitals.Alive()) return;

    player.SetCrouching(Input::GetKey(GLFW_KEY_LEFT_CONTROL));
    player.SetSprinting(Input::GetKey(GLFW_KEY_LEFT_SHIFT) && player.vitals.CanSprint());
    const auto& settings = player.GetSettings();
    const float speed = player.IsCrouching() ? settings.crouchSpeed :
        player.IsSprinting() ? settings.runSpeed : settings.walkSpeed;

    const glm::vec3 forward = glm::normalize(glm::vec3(camera.front.x, 0, camera.front.z));
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 movement(0);
    if (Input::GetKey(GLFW_KEY_W)) movement += forward;
    if (Input::GetKey(GLFW_KEY_S)) movement -= forward;
    if (Input::GetKey(GLFW_KEY_D)) movement += right;
    if (Input::GetKey(GLFW_KEY_A)) movement -= right;
    player.SetMovement(movement, speed);
}
