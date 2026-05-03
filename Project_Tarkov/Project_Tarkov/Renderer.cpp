#include "pch.h"
#include "Renderer.h"

void Renderer::BeginFrame()
{
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::EndFrame(GLFWwindow* window)
{
    glfwSwapBuffers(window);
}

void Renderer::Draw(Shader& shader, Mesh& mesh, Camera& camera, glm::mat4 model, float width, float height)
{
    shader.Bind();
    
    shader.SetVec3(
        "lightPos",
        glm::vec3(5, 10, 5));

    shader.SetVec3(
        "viewPos",
        camera.position);

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjection(width, height);

    shader.SetMat4("model", model);
    shader.SetMat4("view", view);
    shader.SetMat4("projection", proj);

    mesh.Draw();
}

void Renderer::DrawCube(
    Shader& shader,
    Mesh& cubeMesh,
    Camera& camera,
    glm::mat4 model,
    float width,
    float height)
{
    shader.Bind();

    shader.SetMat4("model", model);
    shader.SetMat4("view", camera.GetViewMatrix());
    shader.SetMat4("projection",
        camera.GetProjection(width, height));

    cubeMesh.Draw();
}
