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
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 fogColor;

vec3 SrgbToLinear(vec3 value)
{
    return mix(value / 12.92, pow((value + 0.055) / 1.055, vec3(2.4)),
        greaterThan(value, vec3(0.04045)));
}

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    return mix(value * 12.92, 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055,
        greaterThan(value, vec3(0.0031308)));
}

vec3 AcesToneMap(vec3 value)
{
    return clamp((value * (2.51 * value + 0.03)) /
        (value * (2.43 * value + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 albedo = SrgbToLinear(objectColor);
    if (textured) {
        if (Garment >= 1 && Garment <= 4 && (clothingMask & (1 << (Garment - 1))) == 0) discard;
        // 5 is the body; 6/7 are independent carrier/helmet slots 4/5.
        if (Garment >= 6 && Garment <= 7 && (clothingMask & (1 << (Garment - 2))) == 0) discard;
        // Inner layers remain equipped underneath outerwear without intersecting it.
        if (Garment == 3 && (clothingMask & 1) != 0) discard;
        if (Garment == 4 && (clothingMask & 2) != 0) discard;
        vec4 texel = texture(diffuseTexture, UV);
        // Permanent dark base layer replaces masked skin when the shirt is removed.
        if (Garment == 5 && (clothingMask & 1) == 0 && texel.a < 0.35)
            texel = vec4(SrgbToLinear(vec3(0.12, 0.14, 0.17)), 1.0);
        if (texel.a < 0.35) discard;
        albedo *= texel.rgb;
    }

    vec3 norm = normalize(Normal);
    vec3 sunDir = normalize(-lightDirection);
    float sunDiffuse = max(dot(norm, sunDir), 0.0);
    float fillDiffuse = max(dot(norm, normalize(lightPos - FragPos)), 0.0);
    float skyFactor = clamp(norm.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient = mix(vec3(0.12, 0.13, 0.13), vec3(0.30, 0.32, 0.34), skyFactor);
    vec3 lighting = ambient + lightColor * sunDiffuse * 0.9 + vec3(0.12) * fillDiffuse;
    vec3 color = albedo * lighting;

    float distanceToCamera = length(viewPos - FragPos);
    float fogAmount = smoothstep(85.0, 235.0, distanceToCamera);
    color = mix(color, SrgbToLinear(fogColor), fogAmount);

    FragColor = vec4(LinearToSrgb(AcesToneMap(color)), 1.0);
}
