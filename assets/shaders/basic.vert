#version 430

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
    mat4 model;
    mat3 norm;
    vec4 viewPos;
};

void main() {
    gl_Position = perspective * view * model * vec4(position, 1.0f);
    fragTexCoord = texCoord;
    fragNormal = norm * normal;
    fragPos = vec3(model * vec4(position, 1.0f));
    fragViewPos = viewPos.rgb;
}
