#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Logger.h"

class Window
{
private:
    GLFWwindow* handle = nullptr;
    bool initialized = false;

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
    Window() = default;
    ~Window() { Destroy(); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const char* title)
    {
        Destroy();
        if (!glfwInit()) return false;
        initialized = true;

        // â ���� ���� 16:9
        glfwWindowHint(
            GLFW_RESIZABLE,
            GLFW_TRUE);

        // The scene shaders and texture-buffer skinning require OpenGL 3.3.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);

        handle =
            glfwCreateWindow(
                width,
                height,
                title,
                nullptr,
                nullptr);

        if (!handle)
        {
            Destroy();
            return false;
        }

        glfwMakeContextCurrent(handle);

        if (!gladLoadGLLoader(
            (GLADloadproc)
            glfwGetProcAddress))
        {
            Destroy();
            return false;
        }

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

            if (!monitor) { fullscreen = false; return; }
            const GLFWvidmode* mode =
                glfwGetVideoMode(
                    monitor);
            if (!mode) { fullscreen = false; return; }

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
        if (handle) glfwDestroyWindow(handle);
        handle = nullptr;
        if (initialized) glfwTerminate();
        initialized = false;
    }
};
