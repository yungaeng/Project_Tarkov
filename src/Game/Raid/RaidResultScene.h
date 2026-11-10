#pragma once

#include "../../Scene/Scene.h"
#include "../Hideout/Hideout.h"
#include "../UI/RaidHud.h"
#include "../Character/Player.h"
#include "../../Animation/CharacterVisual.h"
#include "../../Graphics/Shader.h"
#include "RaidSummary.h"

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
    Character rescuedCharacter;
    CharacterVisual rescuedVisual;
    Shader shader;
    RaidHud hud;
    bool returnRequested = false;
};
