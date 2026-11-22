#version 450

// Fullscreen pass: ambient plus every directional light.
layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

#include "include/globals.glsl"
#include "include/gbuffer.glsl"
#include "include/pbr.glsl"
#include "include/lights.glsl"

layout(std140, binding = 2) uniform Lights {
    GpuLight lights[32];
    ivec4 lightCounts;  // x total, y directional
};

#include "include/shadows.glsl"

layout(binding = 5) uniform sampler2D gAlbedoOcclusion;
layout(binding = 6) uniform sampler2D gNormalRoughnessMetallic;
layout(binding = 7) uniform sampler2D gDepth;

layout(binding = 10) uniform samplerCube uIrradianceMap;
layout(binding = 11) uniform samplerCube uPrefilterMap;
layout(binding = 12) uniform sampler2D uBrdfLut;

const float MAX_REFLECTION_LOD = 4.0;

void main() {
    float depth = texture(gDepth, fragTexCoord).r;
    if (depth >= 1.0) discard;  // background keeps the clear color or skybox

    vec4 albedoOcclusion = texture(gAlbedoOcclusion, fragTexCoord);
    vec4 nrm = texture(gNormalRoughnessMetallic, fragTexCoord);
    vec3 albedo = albedoOcclusion.rgb;
    vec3 N = decodeNormal(nrm.xy);
    vec3 P = reconstructWorldPosition(fragTexCoord, depth);
    vec3 V = normalize(viewPos.xyz - P);
    float roughness = nrm.z;
    float metallic = nrm.w;

    vec3 color;
    if (cameraParams.w > 0.0) {
        vec3 R = reflect(-V, N);
        float NoV = clamp(dot(N, V), 0.0, 1.0);
        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 prefiltered =
            textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(uBrdfLut, vec2(NoV, roughness)).rg;
        color = evaluateIBL(N, V, albedo, metallic, roughness,
                            albedoOcclusion.a, irradiance, prefiltered, brdf,
                            cameraParams.w);
    } else {
        color = evaluateAmbient(N, V, albedo, metallic, roughness,
                                albedoOcclusion.a, ambientAndExposure.rgb);
    }
    for (int i = 0; i < lightCounts.y; i++) {
        vec3 L;
        vec3 radiance = incidentRadiance(lights[i], P, L);
        float visibility = lightShadow(lights[i], P, N);
        color += evaluateBRDF(N, V, L, albedo, metallic, roughness) * radiance *
                 visibility;
    }
    outColor = vec4(color, 1.0);
}
