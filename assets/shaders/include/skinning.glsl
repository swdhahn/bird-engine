// Linear blend skinning; mirrors bird::SkinningUBOData (binding 4).
//
// Each palette entry is model * inverseBind for one joint, stored as the
// affine matrix's three rows, so a point transforms as vec4(p, 1) * entry.
// 256 entries (12 KB) fit the 16 KB uniform block minimum of GL ES 3.

layout(location = 4) in uvec4 jointIndices;
layout(location = 5) in vec4 jointWeights;

layout(std140, binding = 4) uniform Skinning {
    mat3x4 joints[256];
} skinning;

// Weighted blend of the vertex's four joint transforms (weights sum to 1).
mat3x4 skinMatrix() {
    return skinning.joints[jointIndices.x] * jointWeights.x +
           skinning.joints[jointIndices.y] * jointWeights.y +
           skinning.joints[jointIndices.z] * jointWeights.z +
           skinning.joints[jointIndices.w] * jointWeights.w;
}

vec3 skinPoint(mat3x4 skin, vec3 p) {
    return vec4(p, 1.0) * skin;
}

// Directions ignore translation. Using the blended upper 3x3 for normals is
// exact for rotations and uniform scale, the overwhelmingly common case.
vec3 skinDirection(mat3x4 skin, vec3 d) {
    return vec4(d, 0.0) * skin;
}
