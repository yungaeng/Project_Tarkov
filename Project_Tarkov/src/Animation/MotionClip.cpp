#include "../Core/RuntimePaths.h"
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
    std::ifstream file(RuntimePaths::Asset(path));
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

MotionPose MotionClip::Sample(const std::string& channel, float time, bool continuous) const
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
    if (continuous) {
        // Monotone cubic Hermite interpolation: retain momentum at intermediate
        // fall keys, but stop at endpoints and direction changes (ground impact).
        const size_t index = static_cast<size_t>(next - keys.begin());
        auto sample = [&](auto member) {
            auto tangent = [&](size_t i, int axis) {
                if (i == 0 || i + 1 == keys.size()) return 0.f;
                const float before = keys[i].time - keys[i - 1].time;
                const float after = keys[i + 1].time - keys[i].time;
                const float a = ((keys[i].pose.*member)[axis] - (keys[i - 1].pose.*member)[axis]) / before;
                const float b = ((keys[i + 1].pose.*member)[axis] - (keys[i].pose.*member)[axis]) / after;
                if (a * b <= 0.f) return 0.f;
                const float w1 = 2.f * after + before, w2 = after + 2.f * before;
                return (w1 + w2) / (w1 / a + w2 / b);
            };
            const float t2 = alpha * alpha, t3 = t2 * alpha;
            const float span = next->time - previous.time;
            glm::vec3 result;
            for (int axis = 0; axis < 3; ++axis)
                result[axis] = (2.f * t3 - 3.f * t2 + 1.f) * (previous.pose.*member)[axis] +
                    (t3 - 2.f * t2 + alpha) * span * tangent(index - 1, axis) +
                    (-2.f * t3 + 3.f * t2) * (next->pose.*member)[axis] +
                    (t3 - t2) * span * tangent(index, axis);
            return result;
        };
        return {sample(&MotionPose::position), sample(&MotionPose::rotation)};
    }
    alpha = alpha * alpha * (3 - 2 * alpha);
    return {glm::mix(previous.pose.position, next->pose.position, alpha),
        glm::mix(previous.pose.rotation, next->pose.rotation, alpha)};
}
