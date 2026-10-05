#pragma once

#include "Scene.h"
#include "Renderer.h"
#include "Player.h"
#include "Input.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "SkeletalAnimation.h"
#include "Texture.h"
#include <utility>
#include <stdexcept>

class RaidScene : public Scene
{
public:
    CollisionWorld collisionWorld;
    std::unique_ptr<Player> player;
    bool inventoryOpen = false;

    Shader shader;
    Mesh cubeMesh;
    Mesh playerMesh;
    Texture  texture;
    SkeletalAnimation animation;
    std::vector<Vertex> animatedVertices;
    bool crouching = false;
    float facing = -90.0f;
    Camera camera;

    void Init() override
    {
        collisionWorld.Clear();
        collisionWorld.AddBox({ { -100, -1.1f, -100 }, { 100, -0.9f, 100 } });
        // Visible static obstacles share exactly the same bounds as collision geometry.
        collisionWorld.AddBox({ { -3, -0.9f, -6 }, { 3, 2.1f, -5.5f } });
        collisionWorld.AddBox({ { 4, -0.9f, -3 }, { 6, 1.1f, -1 } });
        player = std::make_unique<Player>();
        player->SetCollisionWorld(&collisionWorld);
        player->position = { 0, 1.0f, 0 };

        camera.position = { 0,6,10 };

        if (!shader.LoadFromFile(
            "Assets/Shaders/cube.vs",
            "Assets/Shaders/cube.fs")) throw std::runtime_error("Shader load failed");
        cubeMesh.CreateCube();

        if (!animation.LoadModel("Assets/Models/Player/Ch22_nonPBR.fbx") ||
            !animation.LoadClip("idle", "Assets/Idle.fbx") ||
            !animation.LoadClip("walk", "Assets/Walking.fbx") ||
            !animation.LoadClip("run", "Assets/Fast Run.fbx") ||
            !animation.LoadClip("crouch", "Assets/Crouched Walking.fbx"))
            throw std::runtime_error(animation.Error());

        animation.Update(0.15f, "idle", true);
        CopyAnimatedVertices();
        playerMesh.Create(animatedVertices, true);
    }

    void CopyAnimatedVertices()
    {
        const auto& vertices = animation.Vertices();
        animatedVertices.resize(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const auto& source = vertices[i];
            animatedVertices[i].pos = { source.position.x, source.position.y, source.position.z };
            animatedVertices[i].normal = { source.normal.x, source.normal.y, source.normal.z };
            animatedVertices[i].uv = { source.uv.x, source.uv.y };
        }
    }
    void Update(float dt) override
    {
        float eyeHeight = crouching ? 1.0f : 1.7f;
        const glm::vec3 previousPosition = player->position;
        bool sprinting = false;
        player->StopMovement();

        // I ��� �κ��丮
        if (Input::GetKeyDown(GLFW_KEY_I))
        {
            inventoryOpen = !inventoryOpen;
           
            Input::SetCursorCaptured(!inventoryOpen);
        }

        if (!inventoryOpen && Input::IsFocused())
        {
            // ���콺 ȸ��
            const glm::vec2 delta = Input::GetMouseDelta();
            const float dx = delta.x;
            const float dy = delta.y;

            float sens = 0.1f;

            camera.yaw += dx * sens;
            camera.pitch += dy * sens;

            if (camera.pitch > 89.0f)
                camera.pitch = 89.0f;

            if (camera.pitch < -89.0f)
                camera.pitch = -89.0f;

            camera.UpdateDirection();

            // �̵� �ӵ�
            float moveSpeed = 5.0f;

            // Shift �޸���
            sprinting = Input::GetKey(GLFW_KEY_LEFT_SHIFT);
            if (sprinting) moveSpeed = 9.0f;
            // Ctrl �ɱ�
            bool crouch =
                Input::GetKey(GLFW_KEY_LEFT_CONTROL);

            crouching = crouch;
            eyeHeight = crouch ? 1.0f : 1.7f;

            if (crouch)
                moveSpeed = 2.5f;

            glm::vec3 forward =
                glm::normalize(
                    glm::vec3(
                        camera.front.x,
                        0,
                        camera.front.z));

            glm::vec3 right =
                glm::normalize(
                    glm::cross(
                        forward,
                        glm::vec3(0, 1, 0)));

            glm::vec3 movement = { 0, 0, 0 };
            if (Input::GetKey(GLFW_KEY_W)) movement += forward;
            if (Input::GetKey(GLFW_KEY_S)) movement -= forward;
            if (Input::GetKey(GLFW_KEY_D)) movement += right;
            if (Input::GetKey(GLFW_KEY_A)) movement -= right;
            player->SetMovement(movement, moveSpeed);
        }

        // 3��Ī ī�޶� ����
        player->Update(dt);
        const glm::vec3 displacement = player->position - previousPosition;
        const float horizontalDistance = glm::length(glm::vec3(displacement.x, 0, displacement.z));
        const bool moving = horizontalDistance > 0.00001f && player->IsGrounded();
        if (moving)
            facing = glm::degrees(std::atan2(displacement.x, displacement.z));
        animation.Update(dt, crouching ? "crouch" : moving ? (sprinting ? "run" : "walk") : "idle", moving || !crouching);
        CopyAnimatedVertices();
        playerMesh.UpdateVertices(animatedVertices);

        camera.position =
            player->position
            - camera.front * 6.0f
            + glm::vec3(0, eyeHeight + 1.0f, 0);
    }

    void Render() override
    {
        shader.HotReload();
        
        Renderer::BeginFrame();

        int w = 1280, h = 720;
        glfwGetFramebufferSize(
            glfwGetCurrentContext(),
            &w, &h);
        if (w <= 0 || h <= 0) return;

        // �ٴ�
        for (const auto& box : collisionWorld.GetBoxes())
        {
            const glm::vec3 center = (box.min + box.max) * 0.5f;
            const glm::vec3 size = box.max - box.min;
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), center);
            transform = glm::scale(transform, size);
            Renderer::Draw(shader, cubeMesh, camera, transform, (float)w, (float)h);
        }
        glm::mat4 model =glm::translate(glm::mat4(1.0f),player->position);
        model =glm::scale(model,glm::vec3(0.01f));
        model = glm::rotate(model, glm::radians(facing), glm::vec3(0, 1, 0));
        Renderer::Draw(shader,playerMesh,camera,model,(float)w,(float)h);

        //  �÷��̾� ��ġ �α�
       /* std::cout << "Player X: "
        << player->position.x
            << "\n";*/
    }

    void Shutdown() override
    {
        player.reset();
        animation.Reset();
        animatedVertices.clear();
        collisionWorld.Clear();
        texture.Reset();
        playerMesh.Reset();
        cubeMesh.Reset();
        shader.Reset();
    }
};