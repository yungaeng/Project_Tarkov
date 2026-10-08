#pragma once
#include "../Scene/Scene.h"
#include "../Physics/CollisionWorld.h"
#include "Player.h"
#include "PlayerController.h"
#include "LootSystem.h"
#include "RaidHud.h"
#include "InventoryActions.h"
#include "CombatSystem.h"
#include "../Graphics/ThirdPersonCameraController.h"
#include "../Animation/CharacterVisual.h"
#include "../Graphics/Shader.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Camera.h"
#include <memory>
#include "IndustrialGeometry.h"
#include "RaidStatus.h"
#include "Hideout.h"

class RaidScene : public Scene
{
public:
    void Init() override;
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override;

private:
    // Declared before the player so the collision world outlives it.
    CollisionWorld collisionWorld;
    std::unique_ptr<Player> player;
    PlayerController playerController;
    ThirdPersonCameraController cameraController;
    CharacterVisual playerVisual;
    Shader shader;
    Mesh cubeMesh;
    Camera camera;
    LootSystem loot;
    RaidHud hud;
    InventoryActions actions;
    CombatSystem combat;
    unsigned focusGeneration = 0;
    std::vector<IndustrialZone::Batch> mapBatches;
    RaidStatus raid;
    Hideout hideout;
    bool deployRequested = false;
    void StartRaid();
    void FinishRaid(RaidPhase result);
};
