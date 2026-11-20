#version 450

#include "include/mesh_vertex.glsl"

void main() {
    emitMeshVertex(position, normal, tangent);
}
