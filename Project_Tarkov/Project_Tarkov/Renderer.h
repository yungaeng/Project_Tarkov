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
    static void BeginFrame();
    static void EndFrame(GLFWwindow* window);

    static void Draw(
        Shader& shader,
        Mesh& mesh,
        Camera& camera,
        glm::mat4 model,
        float width,
        float height);

    static void DrawCube(
        Shader& shader,
        Mesh& cubeMesh,
        Camera& camera,
        glm::mat4 model,
        float width,
        float height);
};