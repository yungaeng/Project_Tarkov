#pragma once
#include "../Graphics/Mesh.h"
#include "SkeletalAnimation.h"
#include <vector>

class Character;
class Camera;
class Shader;

class CharacterVisual
{
public:
    void Init();
    void Update(float dt, const Character& character);
    void Render(const Character& character, Shader& shader, Camera& camera, float width, float height);
    void Reset();

private:
    void CopyAnimatedVertices();
    SkeletalAnimation animation;
    Mesh mesh;
    std::vector<Vertex> vertices;
};
