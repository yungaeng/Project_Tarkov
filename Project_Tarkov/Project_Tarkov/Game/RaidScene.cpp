#include "RaidScene.h"
#include "../Core/Input.h"
#include "../Graphics/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <cmath>

void RaidScene::Init()
{
    collisionWorld.Clear();
    collisionWorld.AddBox({ { -100, -1.1f, -100 }, { 100, -0.9f, 100 } });
    collisionWorld.AddBox({ { -3, -0.9f, -6 }, { 3, 2.1f, -5.5f } });
    collisionWorld.AddBox({ { 4, -0.9f, -3 }, { 6, 1.1f, -1 } });
    player = std::make_unique<Player>();
    player->SetCollisionWorld(&collisionWorld);
    player->position = { 0, 1.0f, 0 };
    playerController.Reset();
    camera = Camera{};
    cameraController.Follow(camera, *player);
    if (!shader.LoadFromFile("Assets/Shaders/cube.vs", "Assets/Shaders/cube.fs"))
        throw std::runtime_error("Shader load failed");
    cubeMesh.CreateCube();
    playerVisual.Init();
    loot.Init();
    combat.Init(collisionWorld);
    actions.Cancel();
    focusGeneration = Input::FocusGeneration();
    hud.Init(glfwGetCurrentContext());
}

void RaidScene::Update(float dt)
{
    dt = std::clamp(dt, 0.0f, 0.1f);
    if (focusGeneration != Input::FocusGeneration()) { actions.Cancel(); combat.CancelInput(); }
    focusGeneration = Input::FocusGeneration();
    playerController.UpdateInterface();
    actions.Update(*player, loot, camera, collisionWorld, playerController.IsInventoryOpen() && Input::IsFocused());
    if (playerController.IsGameplayInputEnabled() && player->vitals.Alive())
        cameraController.Rotate(camera, Input::GetMouseDelta());
    playerController.UpdateMovement(*player, camera);
    player->Update(dt);
    player->vitals.Update(dt, player->IsSprinting() && player->IsMoving());
    if (player->position.y < -30) player->vitals.Damage(100);
    cameraController.Follow(camera, *player);
    combat.Update(dt, *player, camera, collisionWorld, loot, playerController.IsGameplayInputEnabled());
    if (player->vitals.Alive() && combat.Equipped())
        player->rotation.y = glm::degrees(std::atan2(camera.front.x, camera.front.z));
    playerVisual.Update(dt, *player, {!player->vitals.Alive(), combat.Equipped(), combat.Aiming(),
        combat.Fired(), combat.ReloadTime(), camera.pitch});
    cameraController.Follow(camera, *player);
    loot.Update(dt, *player, camera, collisionWorld, playerController.IsGameplayInputEnabled() && player->vitals.Alive(), Input::GetKeyDown(GLFW_KEY_F));
}

void RaidScene::Render()
{
    shader.HotReload();
    Renderer::BeginFrame();
    int width = 0, height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0) return;
    for (const auto& box : collisionWorld.GetBoxes())
    {
        const glm::vec3 center = (box.min + box.max) * 0.5f;
        const glm::vec3 size = box.max - box.min;
        glm::mat4 transform = glm::translate(glm::mat4(1), center);
        transform = glm::scale(transform, size);
        Renderer::Draw(shader, cubeMesh, camera, transform,
            static_cast<float>(width), static_cast<float>(height));
    }
    loot.Render(shader, cubeMesh, camera, static_cast<float>(width), static_cast<float>(height));
    playerVisual.Render(*player, shader, camera,
        static_cast<float>(width), static_cast<float>(height));
    combat.Render(shader, camera, static_cast<float>(width), static_cast<float>(height));
    hud.Render(*player, loot, combat, actions, playerController.IsInventoryOpen());
}

void RaidScene::Shutdown()
{
    actions.Cancel();
    hud.Shutdown();
    combat.Reset();
    loot.Reset();
    player.reset();
    playerVisual.Reset();
    collisionWorld.Clear();
    cubeMesh.Reset();
    shader.Reset();
}
