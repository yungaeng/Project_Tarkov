#pragma once
class Mesh
{
private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;

    int vertexCount = 0;

public:
    void CreateTriangle()
    {
        float vertices[] =
        {
             0.0f,  0.5f, 0.0f,
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            3 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(0);
    }
    void CreateCube()
    {
        float vertices[] =
        {
            // 앞면
            -0.5f,-0.5f, 0.5f,
             0.5f,-0.5f, 0.5f,
             0.5f, 0.5f, 0.5f,

             0.5f, 0.5f, 0.5f,
            -0.5f, 0.5f, 0.5f,
            -0.5f,-0.5f, 0.5f,

            // 뒤면
            -0.5f,-0.5f,-0.5f,
            -0.5f, 0.5f,-0.5f,
             0.5f, 0.5f,-0.5f,

             0.5f, 0.5f,-0.5f,
             0.5f,-0.5f,-0.5f,
            -0.5f,-0.5f,-0.5f,

            // 왼쪽
            -0.5f, 0.5f, 0.5f,
            -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f,-0.5f,

            -0.5f,-0.5f,-0.5f,
            -0.5f,-0.5f, 0.5f,
            -0.5f, 0.5f, 0.5f,

            // 오른쪽
             0.5f, 0.5f, 0.5f,
             0.5f,-0.5f,-0.5f,
             0.5f, 0.5f,-0.5f,

             0.5f,-0.5f,-0.5f,
             0.5f, 0.5f, 0.5f,
             0.5f,-0.5f, 0.5f,

             // 위
             -0.5f, 0.5f,-0.5f,
             -0.5f, 0.5f, 0.5f,
              0.5f, 0.5f, 0.5f,

              0.5f, 0.5f, 0.5f,
              0.5f, 0.5f,-0.5f,
             -0.5f, 0.5f,-0.5f,

             // 아래
             -0.5f,-0.5f,-0.5f,
              0.5f,-0.5f,-0.5f,
              0.5f,-0.5f, 0.5f,

              0.5f,-0.5f, 0.5f,
             -0.5f,-0.5f, 0.5f,
             -0.5f,-0.5f,-0.5f
        };

        vertexCount = 36;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(vertices),
            vertices,
            GL_STATIC_DRAW);

        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            3 * sizeof(float),
            (void*)0);

        glEnableVertexAttribArray(0);
    }

    void DrawTriangle()
    {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    void Draw()
    {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }
};


