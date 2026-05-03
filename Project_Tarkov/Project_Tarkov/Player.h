#pragma once
#include "pch.h"

#include "Camera.h"
#include "Input.h"

#include "Entity.h"

class Player : public Entity
{
public:
    Camera camera;

    glm::vec3 position = { 0,0,0 };
    float speed = 5.0f;

    void Update(
        float dt,
        const glm::vec3& forward,
        const glm::vec3& right)
    {
        glm::vec3 move = { 0,0,0 };

        if (Input::GetKey(GLFW_KEY_W))
            move += forward;

        if (Input::GetKey(GLFW_KEY_S))
            move -= forward;

        if (Input::GetKey(GLFW_KEY_D))
            move += right;

        if (Input::GetKey(GLFW_KEY_A))
            move -= right;

        if (glm::length(move) > 0.0f)
            move = glm::normalize(move);

        position += move * speed * dt;
    }
    void SyncCamera()
    {
        camera.position = position + glm::vec3(0, 1.8f, 0);
    }
};