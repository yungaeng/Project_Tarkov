#include "CharacterVisual.h"
#include "../Game/Character.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <cmath>

struct CharacterVisual::Assets
{
    Mesh mesh;
    Texture texture;
    MotionClip deathClip, holdClip, reloadClip, aimClip, recoilClip;
    std::array<MotionClip, 5> stances;
};

CharacterVisual::~CharacterVisual() { Reset(); }

void CharacterVisual::Init()
{
    Reset();
    if (!animation.LoadModel("Assets/Models/Player/Ch22_nonPBR.fbx") ||
        !animation.LoadAppearance("Assets/Models/Player/KimGawon/character.mesh") ||
        !animation.LoadClip("idle", "Assets/Idle.fbx") ||
        !animation.LoadClip("walk", "Assets/Walking.fbx") ||
        !animation.LoadClip("run", "Assets/Fast Run.fbx") ||
        !animation.LoadClip("crouch", "Assets/Crouched Walking.fbx"))
        throw std::runtime_error(animation.Error());
    static std::weak_ptr<Assets> cache;
    assets = cache.lock();
    if (!assets)
    {
        auto shared = std::make_shared<Assets>();
        if (!shared->texture.Load("Assets/Models/Player/KimGawon/character.png"))
            throw std::runtime_error("Character texture load failed");
        shared->deathClip.Load("Assets/Animations/Characters/Death.anim");
        shared->holdClip.Load("Assets/Animations/Characters/RifleHold.anim");
        shared->reloadClip.Load("Assets/Animations/Characters/RifleReload.anim");
        const char* names[] = {"RifleIdle", "RifleWalk", "RifleRun", "RifleCrouchIdle", "RifleCrouchWalk"};
        for (int i = 0; i < 5; ++i)
            shared->stances[i].Load(std::string("Assets/Animations/Characters/") + names[i] + ".anim");
        shared->aimClip.Load("Assets/Animations/Characters/RifleAim.anim");
        shared->recoilClip.Load("Assets/Animations/Characters/RifleRecoil.anim");
        animation.CreateMesh(shared->mesh);
        assets = shared;
        cache = shared;
    }
    weapon.Init();
    animation.Update(0.15f, "idle", true, false);
    animation.RefreshPose();
    GLint limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &limit);
    if (animation.Palette().size() * 4 > static_cast<size_t>(limit))
        throw std::runtime_error("Bone palette exceeds GPU capacity");
    glGenBuffers(1, &paletteBuffer);
    glGenTextures(1, &paletteTexture);
    UploadPalette();
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_BUFFER, paletteTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, paletteBuffer);
    glActiveTexture(GL_TEXTURE0);
}

void CharacterVisual::UploadPalette()
{
    const auto& palette = animation.Palette();
    glBindBuffer(GL_TEXTURE_BUFFER, paletteBuffer);
    // Orphan the small palette buffer to avoid waiting on the preceding draw.
    glBufferData(GL_TEXTURE_BUFFER, palette.size() * sizeof(glm::mat4), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, palette.size() * sizeof(glm::mat4), palette.data());
}

void CharacterVisual::Update(float dt, const Character& character, const CharacterVisualState& next, float poseInterval)
{
    if (!assets) return;
    state = next;
    state.equipped = state.equipped && state.hasWeapon;
    recoilTime = state.fired ? 0 : recoilTime + dt;
    firePending |= state.fired;
    if (state.dead)
    {
        weapon.Update(dt, state.equipped, false, 0, false, false, true);
        if (!dying) { animation.CapturePose(); dying = true; deathTime = 0; }
        else if (deathTime >= assets->deathClip.Duration()) return; // Preserve the corpse, no loop or idle breathing.
        deathTime = (std::min)(assets->deathClip.Duration(), deathTime + dt);
        const auto previousHand = animation.NodePosition("RightHand");
        animation.ApplyMotion(assets->deathClip, deathTime, 1, false, true);
        weaponModel[3] += glm::vec4((animation.NodePosition("RightHand") - previousHand) *
            character.scale * character.GetSettings().modelScale, 0);
        deathRoot = assets->deathClip.Sample("Root", deathTime);
        // Keep the rotated mesh above the feet/support plane, including crouched deaths.
        const auto rotation = MotionPose{glm::vec3(0), deathRoot.rotation}.Matrix();
        float minimum = 0;
        for (const auto& vertex : animation.Vertices())
        {
            const glm::vec3 position(vertex.position.x, vertex.position.y, vertex.position.z);
            minimum = (std::min)(minimum, (rotation * glm::vec4(position * character.scale * character.GetSettings().modelScale, 1)).y);
        }
        deathRoot.position.y = (std::max)(deathRoot.position.y, -minimum + 0.01f);
        UploadPalette();
        return;
    }
    poseElapsed += dt;
    if (poseElapsed < poseInterval) return;
    dt = poseElapsed;
    poseElapsed = 0;
    weapon.Update(dt, state.equipped, state.aiming, state.reloadRemaining, firePending, character.IsMoving(), false, character.IsSprinting() && character.IsMoving() && !state.aiming, character.IsCrouching());
    firePending = false;
    const bool moving = character.IsMoving();
    const bool crouching = character.IsCrouching();
    const char* clip = crouching ? "crouch" : moving ? (character.IsSprinting() ? "run" : "walk") : "idle";
    animation.Update(dt, clip, moving || !crouching, false);
    const float blend = 1 - std::exp(-dt * 12);
    holdWeight += ((state.equipped ? 1.0f : 0.0f) - holdWeight) * blend;
    const bool sprint = moving && character.IsSprinting() && !crouching && !state.aiming;
    const int stance = crouching ? (moving ? 4 : 3) : moving ? (sprint ? 2 : 1) : 0;
    motionClock = std::fmod(motionClock + dt, 924.0f);
    aimWeight += ((state.aiming && !sprint && state.reloadRemaining <= 0 ? 1.f : 0.f) - aimWeight) * blend;
    for (int i = 0; i < 5; ++i) stanceWeights[i] += ((i == stance ? 1.f : 0.f) - stanceWeights[i]) * blend;
    if (holdWeight > 0.001f) animation.ApplyMotion(assets->holdClip, 0, holdWeight, true, false, false);
    for (int i = 0; i < 5; ++i) {
        const auto& motion = assets->stances[i];
        animation.ApplyMotion(motion, std::fmod(motionClock, motion.Duration()),
            stanceWeights[i] * holdWeight * (1 - aimWeight * .7f), false, false, false);
    }
    animation.ApplyMotion(assets->aimClip, 0, aimWeight * holdWeight, false, false, false);
    animation.ApplyMotion(assets->recoilClip, recoilTime, holdWeight, false, false, false);
    if (state.reloadRemaining > 0) animation.ApplyMotion(assets->reloadClip, 2 - state.reloadRemaining, holdWeight, false, false, false);
    animation.RefreshPose();
    if (state.hasWeapon && holdWeight > 0.001f) {
        const glm::vec3 scale = character.scale * character.GetSettings().modelScale;
        // RightArm is the shoulder joint. Anchor the butt pad ahead of the torso,
        // not behind a freely animated right hand.
        const glm::vec3 shoulder = animation.NodePosition("RightArm") * scale + glm::vec3(0.035f, -0.035f, 0.08f);
        auto grip = glm::translate(glm::mat4(1), shoulder);
        grip = glm::rotate(grip, glm::radians(-std::clamp(state.aimPitch, -89.f, 89.f)), glm::vec3(1, 0, 0));
        weaponModel = weapon.ModelMatrix(grip);
        const auto right = glm::vec3(weaponModel * glm::vec4(WeaponVisual::FiringGrip(), 1)) / scale;
        const auto left = glm::vec3(weaponModel * glm::vec4(weapon.SupportGrip(), 1)) / scale;
        const glm::vec3 rightPalm = glm::vec3(weaponModel * glm::vec4(1, 0, 0.4f, 0)) / scale;
        const glm::vec3 leftPalm = glm::vec3(weaponModel * glm::vec4(-0.3f, 0, 1, 0)) / scale;
        animation.SolveArm("Right", right, (shoulder + glm::vec3(-0.3f, -0.4f, 0.1f)) / scale, rightPalm, holdWeight);
        animation.SolveArm("Left", left, (shoulder + glm::vec3(0.5f, -0.4f, 0.1f)) / scale, leftPalm, holdWeight);
        animation.RefreshPose();
    }
    UploadPalette();
}

void CharacterVisual::Render(const Character& character, Shader& shader, Camera& camera, float width, float height, const glm::vec3& color)
{
    if (!assets) return;
    glm::mat4 model = glm::translate(glm::mat4(1), character.position);
    model = glm::rotate(model, glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    if (dying) model *= deathRoot.Matrix();
    const glm::mat4 actor = model;
    model = glm::scale(model, character.scale * character.GetSettings().modelScale);
    // Bone envelopes conservatively enclose the current skinned pose.
    // Padding also covers the attached rifle, magazine and muzzle flash.
    const float weaponPadding = 2.0f;
    if (!Renderer::IsVisible(animation.BoundsMin(), animation.BoundsMax(), model, weaponPadding)) return;
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_BUFFER, paletteTexture);
    glActiveTexture(GL_TEXTURE0);
    Renderer::Draw(shader, assets->mesh, camera, model, width, height, glm::vec3(dying ? 0.65f : 1.f), true, assets->texture.id, static_cast<int>(character.clothingMask));
    if (state.hasWeapon) weapon.Render(actor * weaponModel, shader, camera, width, height);
}

void CharacterVisual::Reset()
{
    if (paletteTexture) glDeleteTextures(1, &paletteTexture);
    if (paletteBuffer) glDeleteBuffers(1, &paletteBuffer);
    paletteTexture = paletteBuffer = 0;
    assets.reset();
    poseElapsed = 0;
    animation.Reset();
    weapon.Reset();
    weaponModel = glm::mat4(1);
    dying = false;
    firePending = false;
    deathTime = holdWeight = motionClock = aimWeight = 0;
    recoilTime = 1;
    stanceWeights = {};
    deathRoot = {};
    state = {};
}
