#pragma once
namespace glm
{
    struct vec3
    {
        float x, y, z;
        vec3& operator+=(const vec3& value)
        {
            x += value.x; y += value.y; z += value.z;
            return *this;
        }
    };
    inline vec3 operator*(const vec3& value, float factor)
    {
        return { value.x * factor, value.y * factor, value.z * factor };
    }
    inline vec3 operator/(const vec3& value, float divisor)
    {
        return { value.x / divisor, value.y / divisor, value.z / divisor };
    }
}
