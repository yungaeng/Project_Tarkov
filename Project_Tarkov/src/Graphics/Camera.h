#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

class Camera
{
public:
    glm::vec3 position = { 0,0,0 };

    float yaw = -90.0f;
    float pitch = 0.0f;
    float fieldOfView = 75.0f;

    glm::vec3 front = { 0,0,-1 };
    glm::vec3 up = { 0,1,0 };

public:
    void UpdateDirection()
    {
        glm::vec3 dir;

        dir.x =
            std::cos(glm::radians(yaw)) *
            std::cos(glm::radians(pitch));

        dir.y =
            std::sin(glm::radians(pitch));

        dir.z =
            std::sin(glm::radians(yaw)) *
            std::cos(glm::radians(pitch));

        front = glm::normalize(dir);
    }

    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(
            position,
            position + front,
            up);
    }

    glm::mat4 GetProjection(
        float w,
        float h)
    {
        return glm::perspective(
            glm::radians(fieldOfView),
            (float)w / (float)h,
            0.1f,
            1000.0f);
    }
};
