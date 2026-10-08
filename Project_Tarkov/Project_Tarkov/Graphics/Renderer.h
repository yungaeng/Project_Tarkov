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
    // Call once after binding the scene framebuffer and before scene Draw calls.
    static void PrepareFrame(Shader& shader, Camera& camera, float width, float height);
    static bool IsVisible(const glm::vec3& minimum, const glm::vec3& maximum,
        const glm::mat4& model, float padding = 0.0f);
    static void EndFrame(GLFWwindow* window);

    static void Draw(
        Shader& shader,
        Mesh& mesh,
        Camera& camera,
        glm::mat4 model,
        float width,
        float height, const glm::vec3& color = glm::vec3(0.3f, 0.8f, 0.4f), bool skinned = false);

    static void DrawCube(
        Shader& shader,
        Mesh& cubeMesh,
        Camera& camera,
        glm::mat4 model,
        float width,
        float height);
};
