#version 450

// Batched CPU billboard particle fragment shader.
// When pc.params.x > 0.5, samples uParticleTexture; otherwise evaluates a
// smooth procedural circular radial falloff so textureless emitters render
// crisp soft particles out of the box.
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uParticleTexture;

layout(push_constant) uniform ParticleParams {
    vec4 params;  // x = 1.0 when textured, 0.0 when procedural circle
} pc;

void main() {
    vec4 color = fragColor;
    if (pc.params.x > 0.5) {
        color *= texture(uParticleTexture, fragTexCoord);
    } else {
        vec2 centered = fragTexCoord * 2.0 - vec2(1.0);
        float distSq = dot(centered, centered);
        if (distSq > 1.0) discard;
        float softEdge = 1.0 - smoothstep(0.25, 1.0, distSq);
        color.a *= softEdge;
    }
    if (color.a <= 1e-4) discard;
    outColor = color;
}
