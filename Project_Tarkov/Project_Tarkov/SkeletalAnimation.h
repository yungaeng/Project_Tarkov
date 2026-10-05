#pragma once
#include <assimp/scene.h>
#include <memory>
#include <string>
#include <vector>

struct AnimatedVertex
{
    aiVector3D position;
    aiVector3D normal;
    aiVector3D uv;
};

// CPU skinning keeps the animation independent of OpenGL and its bone uniform limits.
class SkeletalAnimation
{
public:
    SkeletalAnimation();
    ~SkeletalAnimation();
    SkeletalAnimation(const SkeletalAnimation&) = delete;
    SkeletalAnimation& operator=(const SkeletalAnimation&) = delete;
    bool LoadModel(const std::string& path);
    bool LoadClip(const std::string& name, const std::string& path);
    void Update(float dt, const std::string& clip, bool playing);
    const std::vector<AnimatedVertex>& Vertices() const;
    const std::string& Error() const;
    size_t BoneCount() const;
    void Reset();
private:
    struct Data;
    std::unique_ptr<Data> data;
};
