#pragma once
#include "pch.h"

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh
{
private:
    GLuint vao, vbo;
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

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
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

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

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
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void Create(const std::vector<Vertex>& verts)
    {
        vertexCount =
            (int)verts.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(Vertex) * verts.size(),
            verts.data(),
            GL_STATIC_DRAW);

        glVertexAttribPointer(
            0, 3, GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, pos));

        glEnableVertexAttribArray(0);
    }
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }

    bool LoadOBJ(const char* path)
    {
        std::ifstream file(path);

        std::vector<glm::vec3> pos;
        std::vector<unsigned int> indices;
        std::vector<float> verts;

        std::string line;

        while (getline(file, line))
        {
            std::stringstream ss(line);

            std::string type;
            ss >> type;

            if (type == "v")
            {
                glm::vec3 p;
                ss >> p.x >> p.y >> p.z;
                pos.push_back(p);
            }
            else if (type == "f")
            {
                unsigned int a, b, c;
                ss >> a >> b >> c;

                indices.push_back(a - 1);
                indices.push_back(b - 1);
                indices.push_back(c - 1);
            }
        }

        for (auto i : indices)
        {
            verts.push_back(pos[i].x);
            verts.push_back(pos[i].y);
            verts.push_back(pos[i].z);
        }

        vertexCount =
            (int)indices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(
            GL_ARRAY_BUFFER,
            verts.size() * sizeof(float),
            verts.data(),
            GL_STATIC_DRAW);

        glVertexAttribPointer(
            0, 3, GL_FLOAT,
            GL_FALSE,
            3 * sizeof(float),
            (void*)0);

        glEnableVertexAttribArray(0);

        return true;
    }
};


