#pragma once

#include "Scene.h"
#include "Renderer.h"
#include "Player.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "ModelLoader.h"
#include "Texture.h"

class RaidScene : public Scene
{
public:
    Player* player = nullptr;
    bool inventoryOpen = false;

    Shader shader;
    Mesh cubeMesh;
    Mesh playerMesh;
    Texture  texture;
    Camera camera;

    void Init() override
    {
        player = new Player();

        camera.position = { 0,6,10 };

        shader.LoadFromFile(
            "Assets/Shaders/cube.vs",
            "Assets/Shaders/cube.fs");
        cubeMesh.CreateCube();

        LoadedModel playerModel =
            ModelLoader::LoadFBX(
                "Assets/Models/Player/Ch22_nonPBR.fbx");

        playerMesh = playerModel.mesh;

        if (!playerModel.diffuseTexturePath.empty())
        {
            texture.Load(
                playerModel.diffuseTexturePath
                    .c_str());
        }
    }

    void Update(float dt) override
    {
        float eyeHeight = 1.7f;

        // I ��� �κ��丮
        if (Input::GetKeyDown(GLFW_KEY_I))
        {
            inventoryOpen = !inventoryOpen;
           
            GLFWwindow* win =
                glfwGetCurrentContext();

            if (inventoryOpen)
            {
                glfwSetInputMode(
                    win,
                    GLFW_CURSOR,
                    GLFW_CURSOR_NORMAL);
            }
            else
            {
                glfwSetInputMode(
                    win,
                    GLFW_CURSOR,
                    GLFW_CURSOR_DISABLED);
            }
        }

        if (!inventoryOpen)
        {
            // ���콺 ȸ��
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

            // �̵� �ӵ�
            float moveSpeed = 5.0f;

            // Shift �޸���
            if (Input::GetKey(GLFW_KEY_LEFT_SHIFT))
                moveSpeed = 9.0f;
            // Ctrl �ɱ�
            bool crouch =
                Input::GetKey(GLFW_KEY_LEFT_CONTROL);

            eyeHeight =
                crouch ? 1.0f : 1.7f;

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

            player->speed = moveSpeed;

            // �÷��̾� �̵�
            player->Update(
                dt,
                forward,
                right);
        }

        // 3��Ī ī�޶� ����
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

        // �ٴ�
        glm::mat4 floor =glm::translate(glm::mat4(1.0f),glm::vec3(0, -1, 0));
        floor = glm::scale(floor, glm::vec3(200, 0.2f, 200));

        Renderer::Draw(shader,cubeMesh,camera,floor,(float)w,(float)h);

        // �÷��̾�
        glm::mat4 model =glm::translate(glm::mat4(1.0f),player->position);
        model =glm::scale(model,glm::vec3(0.01f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 1, 0));
        Renderer::Draw(shader,playerMesh,camera,model,(float)w,(float)h);

        //  �÷��̾� ��ġ �α�
       /* std::cout << "Player X: "
        << player->position.x
            << "\n";*/
    }

    void Shutdown() override
    {
        delete player;
    }
};