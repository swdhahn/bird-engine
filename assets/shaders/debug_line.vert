#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Global {
    mat4 perspective;
    mat4 view;
    vec4 viewPos;
};

void main() {
    gl_Position = perspective * view * vec4(position, 1.0);
    // Pull lines slightly towards the camera so wireframes drawn exactly on
    // rendered surfaces win the depth test instead of z-fighting.
    gl_Position.z -= 0.0005 * gl_Position.w;
    fragColor = color;
}
