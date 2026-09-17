#pragma once
#include "pch.h"
#include "Logger.h"

class Window
{
private:
    GLFWwindow* handle = nullptr;

    int width = 1280;
    int height = 720;

    bool fullscreen = false;

    int savedX = 100;
    int savedY = 100;
    int savedW = 1280;
    int savedH = 720;

public:
    static void ResizeCallback(
        GLFWwindow* win,
        int w,
        int h)
    {
        glViewport(0, 0, w, h);
    }

public:
    bool Create(const char* title)
    {
        glfwInit();

        // â ���� ���� 16:9
        glfwWindowHint(
            GLFW_RESIZABLE,
            GLFW_TRUE);

        handle =
            glfwCreateWindow(
                width,
                height,
                title,
                nullptr,
                nullptr);

        if (!handle)
            return false;

        glfwMakeContextCurrent(handle);

        if (!gladLoadGLLoader(
            (GLADloadproc)
            glfwGetProcAddress))
            return false;

        glfwSetFramebufferSizeCallback(
            handle,
            ResizeCallback);

        glfwSetWindowAspectRatio(
            handle,
            16,
            9);

        glViewport(0, 0, width, height);

        glfwSetInputMode(
            handle,
            GLFW_CURSOR,
            GLFW_CURSOR_DISABLED);

        return true;
    }

    // F11 ��üȭ�� ���
    void ToggleFullscreen()
    {
        fullscreen = !fullscreen;

        if (fullscreen)
        {
            glfwGetWindowPos(
                handle,
                &savedX,
                &savedY);

            glfwGetWindowSize(
                handle,
                &savedW,
                &savedH);

            GLFWmonitor* monitor =
                glfwGetPrimaryMonitor();

            const GLFWvidmode* mode =
                glfwGetVideoMode(
                    monitor);

            glfwSetWindowMonitor(
                handle,
                monitor,
                0,
                0,
                mode->width,
                mode->height,
                mode->refreshRate);
        }
        else
        {
            glfwSetWindowMonitor(
                handle,
                nullptr,
                savedX,
                savedY,
                savedW,
                savedH,
                0);
        }
    }

    bool ShouldClose()
    {
        return glfwWindowShouldClose(
            handle);
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

    void GetSize(
        int& w,
        int& h)
    {
        glfwGetFramebufferSize(
            handle,
            &w,
            &h);
    }

    void Destroy()
    {
        glfwDestroyWindow(handle);
        glfwTerminate();
    }
};