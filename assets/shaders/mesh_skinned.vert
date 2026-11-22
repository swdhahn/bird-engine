#version 450

#include "include/mesh_vertex.glsl"
#include "include/skinning.glsl"

void main() {
    mat3x4 skin = skinMatrix();
    emitMeshVertex(skinPoint(skin, position), skinDirection(skin, normal),
                   vec4(skinDirection(skin, tangent.xyz), tangent.w));
}
