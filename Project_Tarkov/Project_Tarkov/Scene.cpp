#include "Scene.h"


#include "Input.h"
#include "Time.h"
#include "Logger.h"

void RaidScene::Init()
{
    Logger::Info("RaidScene Init");
}

void RaidScene::Update()
{
    if (Input::GetKey(GLFW_KEY_W))
        playerX += 5.0f * Time::deltaTime;

    if (Input::GetKey(GLFW_KEY_S))
        playerX -= 5.0f * Time::deltaTime;
}

void RaidScene::Render()
{
    if (!glfwGetCurrentContext())
    {
        std::cout << "No OpenGL Context\n";
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
}

void RaidScene::Shutdown()
{
    Logger::Info("RaidScene Shutdown");
}
