#pragma once
class Camera
{
public:
    glm::vec3 position = glm::vec3(0, 0, 3);
    glm::vec3 forward = glm::vec3(0, 0, -1);

    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(position, position + forward, glm::vec3(0, 1, 0));
    }

    glm::mat4 GetProjection(float width, float height)
    {
        return glm::perspective(
            glm::radians(70.0f),
            width / height,
            0.1f,
            100.0f);
    }
};

