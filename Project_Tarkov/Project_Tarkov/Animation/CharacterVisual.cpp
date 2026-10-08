#include "CharacterVisual.h"
#include "../Game/Character.h"
#include "../Graphics/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

void CharacterVisual::Init()
{
    if (!animation.LoadModel("Assets/Models/Player/Ch22_nonPBR.fbx") ||
        !animation.LoadClip("idle", "Assets/Idle.fbx") ||
        !animation.LoadClip("walk", "Assets/Walking.fbx") ||
        !animation.LoadClip("run", "Assets/Fast Run.fbx") ||
        !animation.LoadClip("crouch", "Assets/Crouched Walking.fbx"))
        throw std::runtime_error(animation.Error());
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

void CharacterVisual::Update(float dt, const Character& character)
{
    const bool moving = character.IsMoving();
    const bool crouching = character.IsCrouching();
    const char* clip = crouching ? "crouch" : moving ? (character.IsSprinting() ? "run" : "walk") : "idle";
    animation.Update(dt, clip, moving || !crouching);
    CopyAnimatedVertices();
    mesh.UpdateVertices(vertices);
}

void CharacterVisual::Render(const Character& character, Shader& shader, Camera& camera, float width, float height)
{
    glm::mat4 model = glm::translate(glm::mat4(1), character.position);
    model = glm::rotate(model, glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    model = glm::scale(model, character.scale * character.GetSettings().modelScale);
    Renderer::Draw(shader, mesh, camera, model, width, height);
}

void CharacterVisual::Reset()
{
    mesh.Reset();
    vertices.clear();
    animation.Reset();
}
