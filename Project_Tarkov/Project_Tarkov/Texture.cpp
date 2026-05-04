#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool Texture::Load(
    const char* path)
{
    int w, h, c;

    unsigned char* data =
        stbi_load(path, &w, &h, &c, 4);

    if (!data)
        return false;

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(
        GL_TEXTURE_2D, 0,
        GL_RGBA,
        w, h, 0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data);

    glGenerateMipmap(
        GL_TEXTURE_2D);

    stbi_image_free(data);

    return true;
}