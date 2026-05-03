#pragma once

#include "Scene.h"
#include "Renderer.h"
#include "Player.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"

class RaidScene : public Scene
{
public:
    Player* player = nullptr;

    Shader shader;
    Mesh cubeMesh;
    Camera camera;

    void Init() override
    {
        player = new Player();

        camera.position = { 0,8,12 };

        shader.LoadFromFile(
            "Assets/Shaders/cube.vs",
            "Assets/Shaders/cube.fs");
        cubeMesh.CreateCube();
    }

    void Update(float dt) override
    {
        // 마우스 회전
        static bool first = true;
        static double lastX = 640;
        static double lastY = 360;
        double x, y;

        glfwGetCursorPos(
            glfwGetCurrentContext(),
            &x,
            &y);

        if (first)
        {
            lastX = x;
            lastY = y;
            first = false;
        }

        float dx = (float)(x - lastX);
        float dy = (float)(lastY - y);

        lastX = x;
        lastY = y;

        float sens = 0.1f;

        camera.yaw += dx * sens;
        camera.pitch += dy * sens;

        if (camera.pitch > 89.0f)
            camera.pitch = 89.0f;

        if (camera.pitch < -89.0f)
            camera.pitch = -89.0f;

        camera.UpdateDirection();

        // 이동 방향 계산
        // y축 제거 (하늘/바닥 안 날게)
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

        // 플레이어 이동
        player->Update(
            dt,
            forward,
            right);

        // FPS 시점
        /*camera.position =
            player->position +
            glm::vec3(0, 1.7f, 0);*/

        // 3인칭 카메라 모드
        camera.position =
        player->position
            - camera.front * 6.0f
            + glm::vec3(0, 3, 0);

        // 캐릭터 몸도 회전시키고 싶으면
        //playerYaw = camera.yaw;
    }

    void Render() override
    {
        shader.HotReload();

        Renderer::BeginFrame();

        // 바닥
        glm::mat4 floor =glm::translate(glm::mat4(1.0f),glm::vec3(0, -1, 0));
        floor = glm::scale(floor, glm::vec3(20, 0.2f, 20));

        Renderer::Draw(shader,cubeMesh,camera,floor,1280,720);

        // 플레이어 큐브
        glm::mat4 model =glm::translate(glm::mat4(1.0f),player->position);
        model = glm::scale(model,glm::vec3(1, 2, 1));

        Renderer::Draw(shader,cubeMesh,camera,model,1280,720);

        //  플레이어 움직임 로그
        std::cout << "Player X: "
        << player->position.x
            << "\n";
    }

    void Shutdown() override
    {
        delete player;
    }
};