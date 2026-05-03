#pragma once
#include "pch.h"

class Input
{
public:
    static GLFWwindow* window;

    static void Init(GLFWwindow* w)
    {
        window = w;
    }

    static bool GetKey(int key)
    {
        return glfwGetKey(window, key) == GLFW_PRESS;
    }
};
