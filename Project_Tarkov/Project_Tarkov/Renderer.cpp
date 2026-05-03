#include "pch.h"
#include "Renderer.h"

void Renderer::Draw(Shader& shader, Mesh& mesh, Camera& camera, glm::mat4 model, float width, float height)
{
    shader.Bind();

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjection(width, height);

    shader.SetMat4("model", model);
    shader.SetMat4("view", view);
    shader.SetMat4("projection", proj);

    mesh.Draw();
}
