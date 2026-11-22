// Shared vertex interface of the mesh programs (mesh.vert and
// mesh_skinned.vert): fixed attribute locations, the varyings the
// G-buffer and forward fragment stages read, and the per-draw constants.

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragTangent;

#include "globals.glsl"

// Per-draw data: a push constant on Vulkan, plain uniforms on OpenGL.
layout(push_constant) uniform ObjectConstants {
    mat4 model;
    mat3 normalMatrix;
} object;

// Writes every output from object-space attributes.
void emitMeshVertex(vec3 localPosition, vec3 localNormal, vec4 localTangent) {
    vec4 world = object.model * vec4(localPosition, 1.0);
    gl_Position = perspective * view * world;
    fragWorldPos = world.xyz;
    fragNormal = object.normalMatrix * localNormal;
    fragTexCoord = texCoord;
    // Tangents transform with the model matrix; a zero tangent stays zero
    // and selects the derivative-based frame in the fragment stage.
    fragTangent = vec4(mat3(object.model) * localTangent.xyz, localTangent.w);
}
