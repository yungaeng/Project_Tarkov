#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 UV;
in vec3 BindPosition;
uniform bool textured;
uniform sampler2D diffuseTexture;
uniform int clothingMask;
flat in int Garment;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 objectColor;

void main()
{
    vec3 color = objectColor;
    if (textured) {
        if (Garment >= 1 && Garment <= 4 && (clothingMask & (1 << (Garment - 1))) == 0) discard;
        // Inner layers remain equipped underneath outerwear without intersecting it.
        if (Garment == 3 && (clothingMask & 1) != 0) discard;
        if (Garment == 4 && (clothingMask & 2) != 0) discard;
        vec4 texel = texture(diffuseTexture, UV);
        // Permanent dark base layer replaces masked skin when the shirt is removed.
        if (Garment == 5 && (clothingMask & 1) == 0 && texel.a < 0.35)
            texel = vec4(0.12, 0.14, 0.17, 1.0);
        if (texel.a < 0.35) discard;
        color *= texel.rgb;
    }

    vec3 norm =
        normalize(Normal);

    vec3 lightDir =
        normalize(lightPos - FragPos);

    float diff =
        max(dot(norm, lightDir), 0.0);

    vec3 ambient =
        (textured ? 0.55 : 0.2) * color;

    vec3 diffuse =
        diff * color * (textured ? 0.45 : 1.0);

    FragColor =
        vec4(ambient + diffuse,1.0);
}
