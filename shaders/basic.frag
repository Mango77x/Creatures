#version 330 core

in vec3 vNormal;
out vec4 FragColor;

uniform vec3 uColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.7));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);

    // Quantize into flat tone bands instead of a smooth gradient — the
    // reference's pixel-art look reads in a handful of discrete shades
    // per surface, not a continuous falloff. See CLAUDE.md's visual reference.
    const float bands = 4.0;
    diffuse = floor(diffuse * bands) / bands;

    vec3 shaded = uColor * (0.35 + 0.65 * diffuse);
    FragColor = vec4(shaded, 1.0);
}
