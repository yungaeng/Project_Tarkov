#pragma once
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct MotionPose
{
    glm::vec3 position{0}, rotation{0}; // Translation and XYZ Euler degrees.
    glm::mat4 Matrix() const;
};

// Editable asset: channel, seconds, translation XYZ, rotation XYZ; # comments.
// Channels are independently sampled; one-shots clamp at the final key.
class MotionClip
{
public:
    void Load(const std::string& path);
    MotionPose Sample(const std::string& channel, float time) const;
    float Duration() const { return duration; }
    const auto& Tracks() const { return tracks; }
private:
    struct Key { float time; MotionPose pose; };
    std::unordered_map<std::string, std::vector<Key>> tracks;
    float duration = 0;
};
