#pragma once
#include <glm/vec3.hpp>
#include <algorithm>
#include <cmath>

// Slab intersection. Direction must be normalized; returns distance along the ray.
inline bool RayBox(const glm::vec3& origin, const glm::vec3& direction,
    const glm::vec3& minimum, const glm::vec3& maximum, float limit, float& distance)
{
    float nearDistance = 0, farDistance = limit;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(direction[axis]) < 0.000001f)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return false;
            continue;
        }
        float a = (minimum[axis] - origin[axis]) / direction[axis];
        float b = (maximum[axis] - origin[axis]) / direction[axis];
        if (a > b) std::swap(a, b);
        nearDistance = (std::max)(nearDistance, a);
        farDistance = (std::min)(farDistance, b);
        if (nearDistance > farDistance) return false;
    }
    distance = nearDistance;
    return true;
}
