#pragma once
#include "pch.h"

// Core
#include "Window.h"
#include "Time.h"
#include "SceneManager.h"
#include "Input.h"

// Game
#include "RaidScene.h"

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
            Input::Update();

            // 3. Update
            sceneManager.Update(time.deltaTime);

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

