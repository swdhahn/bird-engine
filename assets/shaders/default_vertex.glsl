#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;

layout(push_constant, std430) uniform Uniforms
{
    mat4 perspective;
    mat4 view;
    mat4 model;
    vec4 lightDir;
} registerMapped;

void main() {
    gl_Position = registerMapped.perspective * registerMapped.view * registerMapped.model * vec4(position, 1.0f);
    fragTexCoord = texCoord;
    fragNormal = normal;
    fragPos = vec3(registerMapped.model * vec4(position, 1.0f));
}