#version 430

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

//layout(location = 0) uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(fragNormal, 1.0);
}
