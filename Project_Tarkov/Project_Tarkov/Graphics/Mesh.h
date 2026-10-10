#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct SkinVertex
{
    glm::vec3 pos{0}, normal{0};
    glm::vec2 uv{0};
    int garment = 0;
    glm::ivec2 influences{0};
};

class Mesh
{
private:
    GLuint vao = 0, vbo = 0, ebo = 0, weightBuffer = 0, weightTexture = 0;
    int indexCount = 0;
    int vertexCount = 0;

public:
    Mesh() = default;
    ~Mesh() { Reset(); }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept { *this = std::move(other); }
    Mesh& operator=(Mesh&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            vao = std::exchange(other.vao, 0);
            vbo = std::exchange(other.vbo, 0);
            vertexCount = std::exchange(other.vertexCount, 0);
            ebo = std::exchange(other.ebo, 0);
            weightBuffer = std::exchange(other.weightBuffer, 0);
            weightTexture = std::exchange(other.weightTexture, 0);
            indexCount = std::exchange(other.indexCount, 0);
        }
        return *this;
    }
    void Reset()
    {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
        if (weightTexture) glDeleteTextures(1, &weightTexture);
        if (weightBuffer) glDeleteBuffers(1, &weightBuffer);
        vao = vbo = ebo = weightBuffer = weightTexture = 0;
        indexCount = 0;
        vertexCount = 0;
    }
    bool IsValid() const { return vao != 0 && vertexCount > 0; }
    void CreateTriangle()
    {
        float vertices[] =
        {
             0.0f,  0.5f, 0.0f,
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f
        };

        Reset();
        vertexCount = 3;
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
        struct Face
        {
            glm::vec3 normal;
            glm::vec3 corners[4];
        };

        Face faces[] =
        {
            { { 0, 0, 1 },
              { {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
                { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f} } },
            { { 0, 0,-1 },
              { { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f},
                {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f} } },
            { {-1, 0, 0 },
              { {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f},
                {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f} } },
            { { 1, 0, 0 },
              { { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f},
                { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f} } },
            { { 0, 1, 0 },
              { {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
                { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f} } },
            { { 0,-1, 0 },
              { {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
                { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f} } },
        };

        std::vector<Vertex> verts;

        for (const auto& face : faces)
        {
            const glm::vec3* c = face.corners;

            auto pushTri = [&](
                const glm::vec3& a,
                const glm::vec3& b,
                const glm::vec3& c_)
            {
                Vertex v;
                v.normal = face.normal;
                v.uv = { 0, 0 };

                v.pos = a; verts.push_back(v);
                v.pos = b; verts.push_back(v);
                v.pos = c_; verts.push_back(v);
            };

            pushTri(c[0], c[1], c[2]);
            pushTri(c[2], c[3], c[0]);
        }

        Create(verts);
    }
    void DrawTriangle()
    {
        glBindVertexArray(vao);
        if (IsValid()) glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void Create(const std::vector<Vertex>& verts, bool dynamic = false)
    {
        Reset();
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
            dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

        glVertexAttribPointer(
            0, 3, GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, pos));

        glVertexAttribPointer(
            1, 3, GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, normal));

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }
    bool UpdateVertices(const std::vector<Vertex>& vertices)
    {
        if (!IsValid() || vertices.size() != static_cast<size_t>(vertexCount)) return false;
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());
        return true;
    }

    void CreateSkinned(const std::vector<SkinVertex>& vertices,
        const std::vector<unsigned int>& indices, const std::vector<glm::vec2>& weights)
    {
        GLint limit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &limit);
        if (weights.size() > static_cast<size_t>(limit))
            throw std::runtime_error("Skin influence buffer exceeds GPU capacity");
        Reset();
        vertexCount = static_cast<int>(vertices.size());
        indexCount = static_cast<int>(indices.size());
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinVertex), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinVertex), (void*)offsetof(SkinVertex, pos));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinVertex), (void*)offsetof(SkinVertex, normal));
        glVertexAttribIPointer(2, 2, GL_INT, sizeof(SkinVertex), (void*)offsetof(SkinVertex, influences));
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(SkinVertex), (void*)offsetof(SkinVertex, uv));
        glVertexAttribIPointer(4, 1, GL_INT, sizeof(SkinVertex), (void*)offsetof(SkinVertex, garment));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &weightBuffer);
        glBindBuffer(GL_TEXTURE_BUFFER, weightBuffer);
        glBufferData(GL_TEXTURE_BUFFER, weights.size() * sizeof(glm::vec2), weights.data(), GL_STATIC_DRAW);
        glGenTextures(1, &weightTexture);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_BUFFER, weightTexture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, weightBuffer);
        glActiveTexture(GL_TEXTURE0);
    }

    void Draw()
    {
        if (!IsValid()) return;
        glBindVertexArray(vao);
        if (weightTexture)
        {
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_BUFFER, weightTexture);
            glActiveTexture(GL_TEXTURE0);
        }
        if (indexCount) glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }

    bool LoadOBJ(const char* path)
    {
        std::ifstream file(path);
        if (!file) return false;

        std::vector<glm::vec3> pos;
        std::vector<unsigned int> indices;
        std::vector<float> verts;

        std::string line;

        while (std::getline(file, line))
        {
            std::stringstream ss(line);

            std::string type;
            ss >> type;

            if (type == "v")
            {
                glm::vec3 p;
                if (!(ss >> p.x >> p.y >> p.z)) return false;
                pos.push_back(p);
            }
            else if (type == "f")
            {
                unsigned int a, b, c;
                if (!(ss >> a >> b >> c) || a == 0 || b == 0 || c == 0)
                    return false;

                indices.push_back(a - 1);
                indices.push_back(b - 1);
                indices.push_back(c - 1);
            }
        }

        for (auto i : indices)
        {
            if (i >= pos.size()) return false;
            verts.push_back(pos[i].x);
            verts.push_back(pos[i].y);
            verts.push_back(pos[i].z);
        }

        Reset();
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
