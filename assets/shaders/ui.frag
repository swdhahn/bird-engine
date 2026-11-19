#version 450

#include "include/ui_sdf.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in vec4 fragClip;
layout(location = 3) in vec2 fragPosition;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D atlas;

void main() {
    // Clipping in the shader keeps the whole UI in one draw call.
    if (fragPosition.x < fragClip.x || fragPosition.y < fragClip.y ||
        fragPosition.x > fragClip.z || fragPosition.y > fragClip.w) {
        discard;
    }
    out_color = uiShade(fragColor, atlas, fragTexCoord);
}
