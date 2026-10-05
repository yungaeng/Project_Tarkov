#pragma once
#include "pch.h"

// Core
#include "Window.h"
#include "Time.h"
#include "SceneManager.h"
#include "Input.h"
#include "Renderer.h"

// Game
#include "RaidScene.h"

class Application
{
private:
    bool running = true;
    bool shutdown = false;

    Window window;
    SceneManager sceneManager;
    Time time;

public:
    ~Application() { Shutdown(); }

    bool Init()
    {
        Logger::Init();

        if (!window.Create("Project_Tarkov"))
            return false;

        Renderer::Init();
        Input::Init(window.GetNative());
        Time::Init();

        sceneManager.Load(std::make_unique<RaidScene>());

        Logger::Info("Application Init Complete");
        return true;
    }

    void Run()
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

    void Shutdown()
    {
        if (shutdown) return;
        shutdown = true;
        sceneManager.Shutdown();
        Input::Shutdown();
        window.Destroy();
        Logger::Shutdown();
    }
};

