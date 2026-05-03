#pragma once
#include "pch.h"

class Input
{
public:
    static GLFWwindow* window;

    static bool current[512];
    static bool previous[512];

public:
    static void Init(GLFWwindow* w)
    {
        window = w;

        for (int i = 0; i < 512; i++)
        {
            current[i] = false;
            previous[i] = false;
        }
    }

    static void Update()
    {
        for (int i = 0; i < 512; i++)
        {
            previous[i] = current[i];
            current[i] =
                glfwGetKey(window, i) == GLFW_PRESS;
        }
    }

    static bool GetKey(int key)
    {
        return current[key];
    }

    static bool GetKeyDown(int key)
    {
        return current[key] && !previous[key];
    }

    static bool GetKeyUp(int key)
    {
        return !current[key] && previous[key];
    }
};
