#include "WeaponVisual.h"
#include "../Graphics/ModelLoader.h"
#include "../Graphics/Renderer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

struct WeaponVisual::Assets
{
    Mesh rifle, magazine, bolt, muzzle;
    MotionClip idle, aim, fire, reload, equip, holster;
    Assets()
    {
        rifle = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/Rifle.obj").mesh);
        magazine = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/Magazine.obj").mesh);
        bolt = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/Bolt.obj").mesh);
        muzzle = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/MuzzleFlash.obj").mesh);
        if (!rifle.IsValid() || !magazine.IsValid() || !bolt.IsValid() || !muzzle.IsValid())
            throw std::runtime_error("Rifle model asset load failed");
        idle.Load("Assets/Animations/Weapons/Idle.anim");
        aim.Load("Assets/Animations/Weapons/Aim.anim");
        fire.Load("Assets/Animations/Weapons/Fire.anim");
        reload.Load("Assets/Animations/Weapons/Reload.anim");
        equip.Load("Assets/Animations/Weapons/Equip.anim");
        holster.Load("Assets/Animations/Weapons/Holster.anim");
    }
};

void WeaponVisual::Init()
{
    Reset();
    static std::weak_ptr<Assets> cache;
    assets = cache.lock();
    if (!assets) { assets = std::make_shared<Assets>(); cache = assets; }
}

void WeaponVisual::Reset()
{
    assets.reset();
    clock = equipTime = aimWeight = moveWeight = 0;
    recoilTime = 10;
    reloadTime = -1;
    visible = wasEquipped = dead = false;
}

void WeaponVisual::Update(float dt, bool equipped, bool aiming, float reloadRemaining, bool fired, bool moving, bool isDead)
{
    if (!assets) return;
    dead = isDead;
    if (equipped != wasEquipped) { equipTime = 0; wasEquipped = equipped; }
    equipTime += dt;
    visible = equipped || equipTime < assets->holster.Duration();
    clock += dt;
    recoilTime = fired ? 0 : recoilTime + dt;
    reloadTime = reloadRemaining > 0 ? 2.0f - reloadRemaining : -1;
    aimWeight += (std::clamp(aiming && !dead ? 1.0f : 0.0f, 0.0f, 1.0f) - aimWeight) * (std::min)(1.0f, dt * 12);
    moveWeight += ((moving ? 1.0f : 0.0f) - moveWeight) * (std::min)(1.0f, dt * 10);
    if (dead) { recoilTime = 10; reloadTime = -1; }
}

void WeaponVisual::Render(const glm::mat4& grip, Shader& shader, Camera& camera, float width, float height)
{
    if (!assets || !visible) return;
    glm::mat4 model = grip;
    if (!dead)
    {
        auto idle = assets->idle.Sample("Weapon", std::fmod(clock, assets->idle.Duration()));
        idle.position *= 1 + moveWeight;
        model *= idle.Matrix();
        auto aim = assets->aim.Sample("Weapon", aimWeight * assets->aim.Duration());
        model *= aim.Matrix();
        model *= (wasEquipped ? assets->equip : assets->holster).Sample("Weapon", equipTime).Matrix();
        model *= assets->fire.Sample("Weapon", recoilTime).Matrix();
        if (reloadTime >= 0) model *= assets->reload.Sample("Weapon", reloadTime).Matrix();
    }
    Renderer::Draw(shader, assets->rifle, camera, model, width, height, glm::vec3(0.24f, 0.28f, 0.24f));
    const auto magazine = model * assets->reload.Sample("Magazine", reloadTime >= 0 ? reloadTime : 0).Matrix();
    Renderer::Draw(shader, assets->magazine, camera, magazine, width, height, glm::vec3(0.12f, 0.14f, 0.13f));
    Renderer::Draw(shader, assets->bolt, camera, model * assets->fire.Sample("Bolt", recoilTime).Matrix(), width, height, glm::vec3(0.4f));
    if (!dead && recoilTime < 0.055f)
        Renderer::Draw(shader, assets->muzzle, camera, model, width, height, glm::vec3(1, 0.75f, 0.15f));
}
