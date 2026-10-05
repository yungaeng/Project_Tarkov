#pragma once
#include "vec3.hpp"
#include <cmath>
namespace glm
{
    inline float length(const vec3& value)
    {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }
}
