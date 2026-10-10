#include "Application.h"
#include "../Game/RaidScene.h"
#include "Input.h"
#include "../Graphics/Renderer.h"
#include "Logger.h"
#include <memory>

bool Application::Init()
{
    Logger::Init();

    if (!window.Create("프로젝트 타르코프"))
        return false;

    Renderer::Init();
    Input::Init(window.GetNative());
    Time::Init();

    sceneManager.Load(std::make_unique<RaidScene>());

    Logger::Info("Application Init Complete");
    return true;
}

void Application::Run()
{
    while (running &&
        !window.ShouldClose())
    {
        time.Update();

        window.PollEvents();

        Input::Update();

        if (Input::GetKeyDown(GLFW_KEY_Q))
            running = false;

        if (Input::GetKeyDown(GLFW_KEY_F11))
        {
            window.ToggleFullscreen();
            Input::ResetMouse();
        }

        sceneManager.Update(
            Time::deltaTime);

        sceneManager.Render();

        window.SwapBuffers();
    }
}

void Application::Shutdown()
{
    if (shutdown) return;
    shutdown = true;
    sceneManager.Shutdown();
    Input::Shutdown();
    window.Destroy();
    Logger::Shutdown();
}
