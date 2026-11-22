#version 450

// Batched CPU billboard particle vertex shader.
// Vertex positions are already expanded into world-space camera-facing quads.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

#include "include/globals.glsl"

void main() {
    fragTexCoord = inTexCoord;
    fragColor = inColor;
    gl_Position = perspective * view * vec4(inPosition, 1.0);
}
