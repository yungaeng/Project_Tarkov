#pragma once

#include "../Scene/Scene.h"
#include "Hideout.h"
#include "RaidHud.h"
#include "Player.h"
#include "../Animation/CharacterVisual.h"
#include "../Graphics/Shader.h"
#include "../RaidSummary.h"

class SceneManager;

class RaidResultScene : public Scene
{
public:
    RaidResultScene(SceneManager& manager, Hideout hideout, RaidSummary summary);
    void Init() override;
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override;

private:
    SceneManager& sceneManager;
    Hideout hideout;
    RaidSummary summary;
    Player character;
    CharacterVisual visual;
    Shader shader;
    RaidHud hud;
    bool returnRequested = false;
};
