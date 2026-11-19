#version 450

// Forward shading: unlit, force-forward and blended materials.
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

#include "include/globals.glsl"
#include "include/material.glsl"
#include "include/pbr.glsl"
#include "include/lights.glsl"

layout(std140, binding = 2) uniform Lights {
    GpuLight lights[32];
    ivec4 lightCounts;  // x total, y directional
};

#include "include/shadows.glsl"

void main() {
    SurfaceSample s = sampleSurface(fragWorldPos, fragNormal, fragTangent,
                                    fragTexCoord, gl_FrontFacing);
    if ((flags & FLAG_ALPHA_TEST) != 0u && s.albedo.a < alphaCutoff) {
        discard;
    }
    if ((flags & FLAG_UNLIT) != 0u) {
        outColor = vec4(s.albedo.rgb + s.emissive, s.albedo.a);
        return;
    }

    vec3 P = fragWorldPos;
    vec3 V = normalize(viewPos.xyz - P);
    vec3 color = s.emissive +
                 evaluateAmbient(s.normal, V, s.albedo.rgb, s.metallic,
                                 s.roughness, s.occlusion,
                                 ambientAndExposure.rgb);
    for (int i = 0; i < lightCounts.x; i++) {
        vec3 L;
        vec3 radiance = incidentRadiance(lights[i], P, L);
        if (dot(radiance, radiance) <= 0.0) continue;
        float visibility = lightShadow(lights[i], P, s.normal);
        color += evaluateBRDF(s.normal, V, L, s.albedo.rgb, s.metallic,
                              s.roughness) *
                 radiance * visibility;
    }
    outColor = vec4(color, s.albedo.a);
}
