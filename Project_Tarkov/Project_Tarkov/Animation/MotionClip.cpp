#include "MotionClip.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

glm::mat4 MotionPose::Matrix() const
{
    return glm::translate(glm::mat4(1), position) * glm::mat4_cast(glm::quat(glm::radians(rotation)));
}

void MotionClip::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Missing motion asset: " + path);
    tracks.clear();
    duration = 0;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream row(line);
        std::string channel;
        if (!(row >> channel) || channel[0] == '#') continue;
        Key key{};
        auto& p = key.pose;
        if (!(row >> key.time >> p.position.x >> p.position.y >> p.position.z >> p.rotation.x >> p.rotation.y >> p.rotation.z))
            throw std::runtime_error("Invalid motion row: " + path);
        if (!std::isfinite(key.time) || key.time < 0)
            throw std::runtime_error("Invalid motion time: " + path);
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(p.position[i]) || !std::isfinite(p.rotation[i]))
                throw std::runtime_error("Invalid motion transform: " + path);
        auto& keys = tracks[channel];
        if ((!keys.empty() && key.time <= keys.back().time) || (keys.empty() && key.time != 0))
            throw std::runtime_error("Motion keys must begin at zero and increase: " + path);
        keys.push_back(key);
        duration = (std::max)(duration, key.time);
    }
    if (tracks.empty()) throw std::runtime_error("Empty motion asset: " + path);
}

MotionPose MotionClip::Sample(const std::string& channel, float time) const
{
    const auto found = tracks.find(channel);
    if (found == tracks.end()) return {};
    const auto& keys = found->second;
    if (time <= keys.front().time) return keys.front().pose;
    if (time >= keys.back().time) return keys.back().pose;
    auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const Key& key) { return t < key.time; });
    const auto& previous = *(next - 1);
    float alpha = (time - previous.time) / (next->time - previous.time);
    alpha = alpha * alpha * (3 - 2 * alpha);
    return {glm::mix(previous.pose.position, next->pose.position, alpha),
        glm::mix(previous.pose.rotation, next->pose.rotation, alpha)};
}
