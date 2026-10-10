#include "WeaponVisual.h"
#include "../Graphics/ModelLoader.h"
#include "../Graphics/Renderer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

struct WeaponVisual::Assets
{
    Mesh rifle, magazine, bolt, muzzle;
    MotionClip idle, aim, fire, reload, equip, holster, run, crouch;
    Assets()
    {
        rifle = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/Rifle.obj").mesh);
        magazine = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/Magazine.obj").mesh);
        bolt = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/Bolt.obj").mesh);
        muzzle = std::move(ModelLoader::LoadFBX("Assets/Models/Weapons/MuzzleFlash.obj").mesh);
        if (!rifle.IsValid() || !magazine.IsValid() || !bolt.IsValid() || !muzzle.IsValid())
            throw std::runtime_error("Rifle model asset load failed");
        idle.Load("Assets/Animations/Weapons/Idle.anim");
        run.Load("Assets/Animations/Weapons/Run.anim");
        crouch.Load("Assets/Animations/Weapons/Crouch.anim");
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
    sprintWeight = crouchWeight = 0;
    reloadTime = -1;
    visible = wasEquipped = dead = false;
}

void WeaponVisual::Update(float dt, bool equipped, bool aiming, float reloadRemaining, bool fired, bool moving, bool isDead, bool sprinting, bool crouching)
{
    if (!assets) return;
    dead = isDead;
    const float blend = 1 - std::exp(-dt * 12);
    sprintWeight += ((sprinting && equipped && !crouching && reloadRemaining <= 0 ? 1.f : 0.f) - sprintWeight) * blend;
    crouchWeight += ((crouching && equipped ? 1.f : 0.f) - crouchWeight) * blend;
    if (equipped != wasEquipped) { equipTime = 0; wasEquipped = equipped; }
    equipTime += dt;
    visible = equipped || equipTime < assets->holster.Duration();
    clock += dt;
    recoilTime = fired ? 0 : recoilTime + dt;
    reloadTime = reloadRemaining > 0 ? 2.0f - reloadRemaining : -1;
    aimWeight += (std::clamp(aiming && !sprinting && !dead ? 1.0f : 0.0f, 0.0f, 1.0f) - aimWeight) * (std::min)(1.0f, dt * 12);
    moveWeight += ((moving ? 1.0f : 0.0f) - moveWeight) * (std::min)(1.0f, dt * 10);
    if (dead) { recoilTime = 10; reloadTime = -1; }
}

glm::mat4 WeaponVisual::ModelMatrix(const glm::mat4& shoulder) const
{
    if (!assets) return shoulder;
    glm::mat4 model = shoulder;
    if (!dead)
    {
        auto idle = assets->idle.Sample("Weapon", std::fmod(clock, assets->idle.Duration()));
        idle.position *= (1 + moveWeight) * (1 - aimWeight * .8f);
        idle.rotation *= 1 - aimWeight * .8f;
        auto running = assets->run.Sample("Weapon", std::fmod(clock, assets->run.Duration()));
        running.position *= sprintWeight; running.rotation *= sprintWeight;
        model *= running.Matrix();
        auto crouched = assets->crouch.Sample("Weapon", std::fmod(clock, assets->crouch.Duration()));
        crouched.position *= crouchWeight; crouched.rotation *= crouchWeight;
        model *= crouched.Matrix();
        model *= idle.Matrix();
        auto aim = assets->aim.Sample("Weapon", aimWeight * assets->aim.Duration());
        model *= aim.Matrix();
        model *= (wasEquipped ? assets->equip : assets->holster).Sample("Weapon", equipTime).Matrix();
        model *= assets->fire.Sample("Weapon", recoilTime).Matrix();
        if (reloadTime >= 0) model *= assets->reload.Sample("Weapon", reloadTime).Matrix();
    }
    // OBJ length is 1.070 m. Use a 0.943 m AK-74-sized silhouette for all parts.
    constexpr float scale = 0.943f / 1.070f;
    // Rotate around the butt pad, keeping it in front of the shoulder at any aim pitch.
    model = glm::translate(model, -glm::vec3(0, 0.073f, -0.32f) * scale);
    return glm::scale(model, glm::vec3(scale));
}

glm::vec3 WeaponVisual::SupportGrip() const
{
    // Wrist socket: the palm/fingers extend forward underneath the rear handguard.
    const glm::vec3 foregrip(0.035f, 0.025f, 0.09f);
    if (!assets || reloadTime < 0) return foregrip;
    // The support hand follows the detachable magazine, then returns to the handguard.
    const float blend = (std::min)(std::clamp(reloadTime / 0.25f, 0.f, 1.f),
        std::clamp((1.8f - reloadTime) / 0.35f, 0.f, 1.f));
    const auto magazine = assets->reload.Sample("Magazine", reloadTime).Matrix();
    return glm::mix(foregrip, glm::vec3(magazine * glm::vec4(0.04f, -0.13f, 0.08f, 1)), blend);
}

void WeaponVisual::Render(const glm::mat4& model, Shader& shader, Camera& camera, float width, float height)
{
    if (!assets || !visible) return;
    Renderer::Draw(shader, assets->rifle, camera, model, width, height, glm::vec3(0.24f, 0.28f, 0.24f));
    const auto magazine = model * assets->reload.Sample("Magazine", reloadTime >= 0 ? reloadTime : 0).Matrix();
    Renderer::Draw(shader, assets->magazine, camera, magazine, width, height, glm::vec3(0.12f, 0.14f, 0.13f));
    Renderer::Draw(shader, assets->bolt, camera, model * assets->fire.Sample("Bolt", recoilTime).Matrix(), width, height, glm::vec3(0.4f));
    if (!dead && recoilTime < 0.055f)
        Renderer::Draw(shader, assets->muzzle, camera, model, width, height, glm::vec3(1, 0.75f, 0.15f));
}
