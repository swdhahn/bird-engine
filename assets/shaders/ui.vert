#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragColor;

layout(std140, binding = 0) uniform Global {
    mat4 perspective;
    mat4 view;
    mat4 model;
    vec4 viewPos;
};

void main()
{
    gl_Position = perspective * vec4(position, 1.0);
    fragColor = color;
}
