#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 objectColor;

void main()
{
    vec3 color = objectColor;

    vec3 norm =
        normalize(Normal);

    vec3 lightDir =
        normalize(lightPos - FragPos);

    float diff =
        max(dot(norm, lightDir), 0.0);

    vec3 ambient =
        0.2 * color;

    vec3 diffuse =
        diff * color;

    FragColor =
        vec4(ambient + diffuse,1.0);
}
