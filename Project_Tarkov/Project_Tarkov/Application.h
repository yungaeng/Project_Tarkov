#pragma once
#include "pch.h"

#include "Window.h"
#include "Time.h"
#include "SceneManager.h"
#include "Input.h"

class Application
{
private:
    bool running = true;

    Window window;
    SceneManager sceneManager;
    Time time;

public:
    bool Init()
    {
        Logger::Init();

        if (!window.Create("Project_Tarkov"))
            return false;

        Input::Init(window.GetNative());
        Time::Init();

        sceneManager.Load(new RaidScene());

        Logger::Info("Application Init Complete");
        return true;
    }

    void Run()
    {
        while (running && !window.ShouldClose())
        {
            // 1. Time
            time.Update();

            // 2. Input
            window.PollEvents();

            // ESC Á¾·á
            if (Input::GetKey(GLFW_KEY_ESCAPE))
                running = false;

            // 3. Update
            sceneManager.Update();

            // 4. Render
            sceneManager.Render();

            // 5. Present
            window.SwapBuffers();
        }
    }

    void Shutdown()
    {
        sceneManager.Shutdown();
        window.Destroy();
        Logger::Shutdown();
    }
};

