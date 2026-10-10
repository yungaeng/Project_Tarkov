#pragma once
#include <assimp/scene.h>
#include <memory>
#include <string>
#include <vector>
#include "MotionClip.h"
#include "../Graphics/Mesh.h"

struct AnimatedVertex
{
    aiVector3D position;
    aiVector3D normal;
    aiVector3D uv;
};

// Shared assets and per-character poses; GPU skinning retains all bone influences.
class SkeletalAnimation
{
public:
    SkeletalAnimation();
    ~SkeletalAnimation();
    SkeletalAnimation(const SkeletalAnimation&) = delete;
    SkeletalAnimation& operator=(const SkeletalAnimation&) = delete;
    bool LoadModel(const std::string& path);
    bool LoadAppearance(const std::string& path);
    bool LoadClip(const std::string& name, const std::string& path);
    void Update(float dt, const std::string& clip, bool playing, bool skin = true);
    const std::vector<AnimatedVertex>& Vertices() const;
    const std::string& Error() const;
    size_t BoneCount() const;
    void Reset();
    void CapturePose();
    void ApplyMotion(const MotionClip& clip, float time, float weight, bool fromBind = false, bool frozen = false, bool skin = true);
    void RefreshVertices();
    void RefreshPose();
    const std::vector<glm::mat4>& Palette() const;
    glm::vec3 BoundsMin() const;
    glm::vec3 BoundsMax() const;
    void CreateMesh(Mesh& mesh) const;
    glm::vec3 NodePosition(const std::string& name) const;
    void SolveArm(const std::string& side, const glm::vec3& target, const glm::vec3& pole,
        const glm::vec3& palmDirection, float weight);
private:
    struct Data;
    std::unique_ptr<Data> data;
};
