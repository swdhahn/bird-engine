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

void main() {
    float depth = texture(gDepth, fragTexCoord).r;
    if (depth >= 1.0) discard;  // background keeps the clear color

    vec4 albedoOcclusion = texture(gAlbedoOcclusion, fragTexCoord);
    vec4 nrm = texture(gNormalRoughnessMetallic, fragTexCoord);
    vec3 albedo = albedoOcclusion.rgb;
    vec3 N = decodeNormal(nrm.xy);
    vec3 P = reconstructWorldPosition(fragTexCoord, depth);
    vec3 V = normalize(viewPos.xyz - P);

    vec3 color = evaluateAmbient(N, V, albedo, nrm.w, nrm.z,
                                 albedoOcclusion.a, ambientAndExposure.rgb);
    for (int i = 0; i < lightCounts.y; i++) {
        vec3 L;
        vec3 radiance = incidentRadiance(lights[i], P, L);
        float visibility = lightShadow(lights[i], P, N);
        color += evaluateBRDF(N, V, L, albedo, nrm.w, nrm.z) * radiance *
                 visibility;
    }
    outColor = vec4(color, 1.0);
}
