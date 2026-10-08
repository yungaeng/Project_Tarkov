#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>

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
        float height, const glm::vec3& color = glm::vec3(0.3f, 0.8f, 0.4f));

    static void DrawCube(
        Shader& shader,
        Mesh& cubeMesh,
        Camera& camera,
        glm::mat4 model,
        float width,
        float height);
};
