#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    vec3 color = vec3(0.3,0.8,0.4);

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
