#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragTangent;

#include "include/globals.glsl"

// Per-draw data: a push constant on Vulkan, plain uniforms on OpenGL.
layout(push_constant) uniform ObjectConstants {
    mat4 model;
    mat3 normalMatrix;
} object;

void main() {
    vec4 world = object.model * vec4(position, 1.0);
    gl_Position = perspective * view * world;
    fragWorldPos = world.xyz;
    fragNormal = object.normalMatrix * normal;
    fragTexCoord = texCoord;
    // Tangents transform with the model matrix; a zero tangent stays zero
    // and selects the derivative-based frame in the fragment stage.
    fragTangent = vec4(mat3(object.model) * tangent.xyz, tangent.w);
}
