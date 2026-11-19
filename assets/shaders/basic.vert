#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) flat out vec3 fragViewPos;

layout(std140, binding = 0) uniform Global {
    mat4 perspective;
    mat4 view;
    vec4 viewPos;
};

// Per-draw data: a push constant on Vulkan, plain uniforms on OpenGL.
layout(push_constant) uniform ObjectConstants {
    mat4 model;
} object;

void main() {
    gl_Position = perspective * view * object.model * vec4(position, 1.0f);
    fragTexCoord = texCoord;
    fragNormal = normal;
    fragPos = vec3(object.model * vec4(position, 1.0f));
    fragViewPos = viewPos.rgb;
}
