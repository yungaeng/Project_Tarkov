#pragma once
#include "pch.h"

#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"

class Renderer
{
public:
    static void Init()
    {
        glEnable(GL_DEPTH_TEST);
    }

    static void BeginFrame()
    {
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    static void Draw(
        Shader& shader,
        Mesh& mesh,
        Camera& camera,
        glm::mat4 model,
        float width,
        float height);

    static void EndFrame(GLFWwindow* window)
    {
        glfwSwapBuffers(window);
    }
};
