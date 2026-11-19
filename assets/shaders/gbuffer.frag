#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outAlbedoOcclusion;
layout(location = 1) out vec4 outNormalRoughnessMetallic;
layout(location = 2) out vec4 outEmissive;

#include "include/globals.glsl"
#include "include/material.glsl"
#include "include/gbuffer.glsl"

void main() {
    SurfaceSample s = sampleSurface(fragWorldPos, fragNormal, fragTangent,
                                    fragTexCoord, gl_FrontFacing);
    if ((flags & FLAG_ALPHA_TEST) != 0u && s.albedo.a < alphaCutoff) {
        discard;
    }
    outAlbedoOcclusion = vec4(s.albedo.rgb, s.occlusion);
    outNormalRoughnessMetallic =
        vec4(encodeNormal(s.normal), s.roughness, s.metallic);
    // Emissive seeds the HDR accumulation target; lights add on top.
    outEmissive = vec4(s.emissive, 1.0);
}
