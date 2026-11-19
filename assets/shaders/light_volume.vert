#version 450

layout(location = 0) in vec3 position;

#include "include/globals.glsl"

layout(push_constant) uniform LightVolume {
    mat4 model;
    vec4 positionRange;
    vec4 colorType;
    vec4 directionCosOuter;
    vec4 shadowParams;
} volume;

void main() {
    gl_Position = perspective * view * volume.model * vec4(position, 1.0);
}
