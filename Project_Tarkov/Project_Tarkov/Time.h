#pragma once
#include "pch.h"

class Time
{
public:
    static float deltaTime;
    static float lastTime;

    static void Init()
    {
        lastTime = (float)glfwGetTime();
    }

    static void Update()
    {
        float now = (float)glfwGetTime();
        deltaTime = now - lastTime;
        lastTime = now;
    }
};



