#version 450

#include "include/ui_sdf.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D atlas;

void main() {
    out_color = uiShade(fragColor, atlas, fragTexCoord);
}
