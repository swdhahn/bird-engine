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

layout(binding = 10) uniform samplerCube uIrradianceMap;
layout(binding = 11) uniform samplerCube uPrefilterMap;
layout(binding = 12) uniform sampler2D uBrdfLut;

const float MAX_REFLECTION_LOD = 4.0;

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
    vec3 ambient;
    if (cameraParams.w > 0.0) {
        vec3 R = reflect(-V, s.normal);
        float NoV = clamp(dot(s.normal, V), 0.0, 1.0);
        vec3 irradiance = texture(uIrradianceMap, s.normal).rgb;
        vec3 prefiltered =
            textureLod(uPrefilterMap, R, s.roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(uBrdfLut, vec2(NoV, s.roughness)).rg;
        ambient = evaluateIBL(s.normal, V, s.albedo.rgb, s.metallic,
                              s.roughness, s.occlusion, irradiance, prefiltered,
                              brdf, cameraParams.w);
    } else {
        ambient = evaluateAmbient(s.normal, V, s.albedo.rgb, s.metallic,
                                  s.roughness, s.occlusion,
                                  ambientAndExposure.rgb);
    }
    vec3 color = s.emissive + ambient;
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
