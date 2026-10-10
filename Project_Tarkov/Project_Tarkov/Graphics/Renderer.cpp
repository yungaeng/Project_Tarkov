#include "../Core/pch.h"
#include "Renderer.h"
#include <limits>

void Renderer::BeginFrame()
{
    glClearColor(0.20f, 0.23f, 0.24f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::EndFrame(GLFWwindow* window)
{
    glfwSwapBuffers(window);
}

namespace
{
    glm::mat4 frameViewProjection(1);
}

void Renderer::PrepareFrame(Shader& shader, Camera& camera, float width, float height)
{
    const auto view = camera.GetViewMatrix();
    const auto projection = camera.GetProjection(width, height);
    frameViewProjection = projection * view;
    shader.Bind();
    shader.SetMat4("view", view);
    shader.SetMat4("projection", projection);
    shader.SetVec3("lightPos", glm::vec3(5, 10, 5));
    shader.SetVec3("lightDirection", glm::vec3(-0.45f, -1.0f, -0.35f));
    shader.SetVec3("lightColor", glm::vec3(1.0f, 0.91f, 0.78f));
    shader.SetVec3("fogColor", glm::vec3(0.47f, 0.51f, 0.50f));
    shader.SetVec3("viewPos", camera.position);
    shader.SetInt("skinWeights", 6);
    shader.SetInt("bonePalette", 7);
    shader.SetInt("skinned", 0);
}

bool Renderer::IsVisible(const glm::vec3& minimum, const glm::vec3& maximum,
    const glm::mat4& model, float padding)
{
    glm::vec3 low((std::numeric_limits<float>::max)()), high(-(std::numeric_limits<float>::max)());
    for (int corner = 0; corner < 8; ++corner)
    {
        glm::vec3 point;
        for (int axis = 0; axis < 3; ++axis)
            point[axis] = (corner & (1 << axis)) ? maximum[axis] : minimum[axis];
        point = glm::vec3(model * glm::vec4(point, 1));
        low = (glm::min)(low, point);
        high = (glm::max)(high, point);
    }
    low -= glm::vec3(padding);
    high += glm::vec3(padding);
    glm::vec4 corners[8];
    for (int corner = 0; corner < 8; ++corner)
    {
        glm::vec3 point;
        for (int axis = 0; axis < 3; ++axis)
            point[axis] = (corner & (1 << axis)) ? high[axis] : low[axis];
        corners[corner] = frameViewProjection * glm::vec4(point, 1);
    }
    for (int axis = 0; axis < 3; ++axis)
    {
        bool outsideLow = true, outsideHigh = true;
        for (const auto& point : corners)
        {
            outsideLow &= point[axis] < -point.w;
            outsideHigh &= point[axis] > point.w;
        }
        if (outsideLow || outsideHigh) return false;
    }
    return true;
}

void Renderer::Draw(Shader& shader, Mesh& mesh, Camera&, glm::mat4 model,
    float, float, const glm::vec3& color, bool skinned, GLuint texture, int clothingMask)
{
    // PrepareFrame binds the scene shader and uploads frame constants once.
    shader.SetVec3("objectColor", color);
    shader.SetMat4("model", model);
    shader.SetInt("skinned", skinned ? 1 : 0);
    shader.SetInt("textured", texture ? 1 : 0);
    shader.SetInt("clothingMask", clothingMask);
    if (texture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        shader.SetInt("diffuseTexture", 0);
    }
    mesh.Draw();
}

void Renderer::DrawCube(Shader& shader, Mesh& mesh, Camera& camera, glm::mat4 model,
    float width, float height)
{
    Draw(shader, mesh, camera, model, width, height);
}
