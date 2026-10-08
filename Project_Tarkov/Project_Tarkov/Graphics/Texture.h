#pragma once
#include <glad/glad.h>

class Texture
{
public:
    GLuint id = 0;

    Texture() = default;
    ~Texture() { Reset(); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    void Reset()
    {
        if (id) glDeleteTextures(1, &id);
        id = 0;
    }
    bool Load(
        const char* path);

    void Bind(
        int slot = 0);
};
