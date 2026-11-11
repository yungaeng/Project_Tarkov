#pragma once

// Core
#include "Window.h"
#include "Time.h"
#include "../Scene/SceneManager.h"

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

    bool Init();

    void Run();

    void Shutdown();
};
