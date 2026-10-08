#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in ivec2 aInfluences;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool skinned;
uniform samplerBuffer skinWeights;
uniform samplerBuffer bonePalette;

mat4 paletteMatrix(int offset)
{
    return mat4(texelFetch(bonePalette, offset), texelFetch(bonePalette, offset + 1),
        texelFetch(bonePalette, offset + 2), texelFetch(bonePalette, offset + 3));
}

void main()
{
    vec3 position = aPos;
    vec3 normal = aNormal;
    if (skinned)
    {
        position = vec3(0);
        normal = vec3(0);
        for (int i = 0; i < aInfluences.y; ++i)
        {
            vec2 influence = texelFetch(skinWeights, aInfluences.x + i).xy;
            int offset = int(influence.x) * 8;
            position += (paletteMatrix(offset) * vec4(aPos, 1)).xyz * influence.y;
            normal += mat3(paletteMatrix(offset + 4)) * aNormal * influence.y;
        }
        if (dot(normal, normal) > 0) normal = normalize(normal);
    }
    FragPos = vec3(model * vec4(position, 1));
    Normal = mat3(transpose(inverse(model))) * normal;
    gl_Position = projection * view * vec4(FragPos, 1);
}
