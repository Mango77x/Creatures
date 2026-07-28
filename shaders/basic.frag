#version 330 core

in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;

// Tints vColor rather than replacing it: creature meshes carry their real
// per-DNA color in vColor and pass white here, while terrain/walls (which
// have no per-vertex color) carry white in vColor and keep using this
// uniform exactly as before Phase 9. See CreatureMesh.h's MeshVertex::color.
uniform vec3 uColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.7));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);

    // Quantize into flat tone bands instead of a smooth gradient — the
    // reference's pixel-art look reads in a handful of discrete shades
    // per surface, not a continuous falloff. See CLAUDE.md's visual reference.
    const float bands = 4.0;
    diffuse = floor(diffuse * bands) / bands;

    vec3 shaded = (uColor * vColor) * (0.35 + 0.65 * diffuse);
    FragColor = vec4(shaded, 1.0);
}
