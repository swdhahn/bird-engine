#version 450

// Shades one point or spot light over the pixels its volume covers.
layout(location = 0) out vec4 outColor;

#include "include/globals.glsl"
#include "include/gbuffer.glsl"
#include "include/pbr.glsl"
#include "include/lights.glsl"
#include "include/shadows.glsl"

layout(push_constant) uniform LightVolume {
    mat4 model;
    vec4 positionRange;
    vec4 colorType;
    vec4 directionCosOuter;
    vec4 shadowParams;
} volume;

layout(binding = 5) uniform sampler2D gAlbedoOcclusion;
layout(binding = 6) uniform sampler2D gNormalRoughnessMetallic;
layout(binding = 7) uniform sampler2D gDepth;

void main() {
    vec2 uv = gl_FragCoord.xy * screenSize.zw;
    float depth = texture(gDepth, uv).r;
    if (depth >= 1.0) discard;

    GpuLight light;
    light.positionRange = volume.positionRange;
    light.colorType = volume.colorType;
    light.directionCosOuter = volume.directionCosOuter;
    light.shadowParams = volume.shadowParams;

    vec3 P = reconstructWorldPosition(uv, depth);
    vec3 L;
    vec3 radiance = incidentRadiance(light, P, L);
    if (dot(radiance, radiance) <= 0.0) discard;

    vec4 albedoOcclusion = texture(gAlbedoOcclusion, uv);
    vec4 nrm = texture(gNormalRoughnessMetallic, uv);
    vec3 N = decodeNormal(nrm.xy);
    vec3 V = normalize(viewPos.xyz - P);
    float visibility = lightShadow(light, P, N);
    outColor = vec4(evaluateBRDF(N, V, L, albedoOcclusion.rgb, nrm.w, nrm.z) *
                        radiance * visibility,
                    1.0);
}
