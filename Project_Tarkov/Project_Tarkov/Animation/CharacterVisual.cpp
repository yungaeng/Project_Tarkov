#include "CharacterVisual.h"
#include "../Game/Character.h"
#include "../Graphics/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>

void CharacterVisual::Init()
{
    Reset();
    if (!animation.LoadModel("Assets/Models/Player/Ch22_nonPBR.fbx") ||
        !animation.LoadClip("idle", "Assets/Idle.fbx") ||
        !animation.LoadClip("walk", "Assets/Walking.fbx") ||
        !animation.LoadClip("run", "Assets/Fast Run.fbx") ||
        !animation.LoadClip("crouch", "Assets/Crouched Walking.fbx"))
        throw std::runtime_error(animation.Error());
    deathClip.Load("Assets/Animations/Characters/Death.anim");
    holdClip.Load("Assets/Animations/Characters/RifleHold.anim");
    reloadClip.Load("Assets/Animations/Characters/RifleReload.anim");
    weapon.Init();
    animation.Update(0.15f, "idle", true);
    CopyAnimatedVertices();
    mesh.Create(vertices, true);
}

void CharacterVisual::CopyAnimatedVertices()
{
    const auto& source = animation.Vertices();
    vertices.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i)
    {
        vertices[i].pos = { source[i].position.x, source[i].position.y, source[i].position.z };
        vertices[i].normal = { source[i].normal.x, source[i].normal.y, source[i].normal.z };
        vertices[i].uv = { source[i].uv.x, source[i].uv.y };
    }
}

void CharacterVisual::Update(float dt, const Character& character, const CharacterVisualState& next)
{
    state = next;
    weapon.Update(dt, state.equipped, state.aiming, state.reloadRemaining, state.fired, character.IsMoving(), state.dead);
    if (state.dead)
    {
        if (!dying) { animation.CapturePose(); dying = true; deathTime = 0; }
        else if (deathTime >= deathClip.Duration()) return; // Preserve the corpse, no loop or idle breathing.
        deathTime = (std::min)(deathClip.Duration(), deathTime + dt);
        animation.ApplyMotion(deathClip, deathTime, 1, false, true);
        deathRoot = deathClip.Sample("Root", deathTime);
        CopyAnimatedVertices();
        // Keep the rotated mesh above the feet/support plane, including crouched deaths.
        const auto rotation = MotionPose{glm::vec3(0), deathRoot.rotation}.Matrix();
        float minimum = 0;
        for (const auto& vertex : vertices)
            minimum = (std::min)(minimum, (rotation * glm::vec4(vertex.pos * character.scale * character.GetSettings().modelScale, 1)).y);
        deathRoot.position.y = (std::max)(deathRoot.position.y, -minimum + 0.01f);
        mesh.UpdateVertices(vertices);
        return;
    }
    const bool moving = character.IsMoving();
    const bool crouching = character.IsCrouching();
    const char* clip = crouching ? "crouch" : moving ? (character.IsSprinting() ? "run" : "walk") : "idle";
    animation.Update(dt, clip, moving || !crouching, false);
    holdWeight += ((state.equipped ? 1.0f : 0.0f) - holdWeight) * (std::min)(1.0f, dt * 10);
    if (holdWeight > 0.001f) animation.ApplyMotion(holdClip, 0, holdWeight, true, false, false);
    if (state.reloadRemaining > 0) animation.ApplyMotion(reloadClip, 2 - state.reloadRemaining, holdWeight, false, false, false);
    animation.RefreshVertices();
    CopyAnimatedVertices();
    mesh.UpdateVertices(vertices);
}

void CharacterVisual::Render(const Character& character, Shader& shader, Camera& camera, float width, float height, const glm::vec3& color)
{
    glm::mat4 model = glm::translate(glm::mat4(1), character.position);
    model = glm::rotate(model, glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    if (dying) model *= deathRoot.Matrix();
    model = glm::scale(model, character.scale * character.GetSettings().modelScale);
    Renderer::Draw(shader, mesh, camera, model, width, height, dying ? color * 0.65f : color);
    const glm::vec3 hand = glm::vec3(model * glm::vec4(animation.NodePosition("RightHand"), 1));
    auto grip = glm::translate(glm::mat4(1), hand);
    grip = glm::rotate(grip, glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    if (dying) grip *= MotionPose{glm::vec3(0), deathRoot.rotation}.Matrix();
    else grip = glm::rotate(grip, glm::radians(-state.aimPitch), glm::vec3(1, 0, 0));
    weapon.Render(grip, shader, camera, width, height);
}

void CharacterVisual::Reset()
{
    mesh.Reset();
    vertices.clear();
    animation.Reset();
    weapon.Reset();
    dying = false;
    deathTime = holdWeight = 0;
    deathRoot = {};
    state = {};
}
