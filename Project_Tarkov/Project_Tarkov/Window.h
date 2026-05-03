#pragma once
#include "pch.h"
#include "Logger.h"

class Window
{
private:
    GLFWwindow* handle = nullptr;
    int width = 1280;
    int height = 720;

public:
    bool Create(const char* title)
    {
        glfwInit();

        handle = glfwCreateWindow(1280, 720, title, nullptr, nullptr);

        if (!handle)
            return false;

        glfwMakeContextCurrent(handle);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            return false;

        return true;
    }

    bool ShouldClose()
    {
        return glfwWindowShouldClose(handle);
    }

    void PollEvents()
    {
        glfwPollEvents();
    }

    void SwapBuffers()
    {
        glfwSwapBuffers(handle);
    }

    GLFWwindow* GetNative()
    {
        return handle;
    }

    void Destroy()
    {
        glfwDestroyWindow(handle);
        glfwTerminate();
    }
};
