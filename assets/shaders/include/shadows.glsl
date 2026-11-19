// Shadow projections; mirrors bird::ShadowUBOData (binding 3).
// Requires globals.glsl and lights.glsl.
layout(std140, binding = 3) uniform Shadows {
    mat4 shadowMatrices[64];
    vec4 shadowAtlasRects[64];   // xy offset, zw scale
    vec4 shadowMatrixParams[64]; // x texel size, y perspective flag
    vec4 cascadeSplits;          // far view distance per cascade
    vec4 shadowInfo;             // x cascades, y 1/dir atlas, z 1/local atlas
};

layout(binding = 8) uniform sampler2DShadow directionalShadowAtlas;
layout(binding = 9) uniform sampler2DShadow localShadowAtlas;

// Mirrors bird::shadow::selectCubeFace: +X, -X, +Y, -Y, +Z, -Z.
int selectCubeFace(vec3 v) {
    vec3 a = abs(v);
    if (a.x >= a.y && a.x >= a.z) return v.x >= 0.0 ? 0 : 1;
    if (a.y >= a.z) return v.y >= 0.0 ? 2 : 3;
    return v.z >= 0.0 ? 4 : 5;
}

// 3x3 hardware-filtered PCF, clamped inside the tile so that taps never
// read a neighbouring tile.
float filterShadow(bool directional, int index, vec3 tileCoord, float texel) {
    vec4 rect = shadowAtlasRects[index];
    vec2 lo = rect.xy + vec2(texel * 1.5);
    vec2 hi = rect.xy + rect.zw - vec2(texel * 1.5);
    vec2 center = rect.xy + tileCoord.xy * rect.zw;
    float lit = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec3 coord = vec3(clamp(center + vec2(x, y) * texel, lo, hi),
                              tileCoord.z);
            lit += directional ? texture(directionalShadowAtlas, coord)
                               : texture(localShadowAtlas, coord);
        }
    }
    return lit / 9.0;
}

// Projects a normal-offset receiver into one shadow matrix and filters.
float sampleShadowMatrix(bool directional, int index, vec3 P, vec3 N,
                         float distanceToLight, GpuLight light) {
    vec4 params = shadowMatrixParams[index];
    bool perspectiveProjection = params.y > 0.5;
    float worldTexel =
        perspectiveProjection ? params.x * distanceToLight : params.x;
    vec3 offsetP = P + N * (light.shadowParams.w * worldTexel);
    vec4 clip = shadowMatrices[index] * vec4(offsetP, 1.0);
    vec3 coord = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(coord, vec3(0.0))) || any(greaterThan(coord, vec3(1.0)))) {
        return 1.0;
    }
    float depthBias = light.shadowParams.z *
                      (perspectiveProjection ? 0.00002 : 0.0002);
    coord.z -= depthBias;
    float texel = directional ? shadowInfo.y : shadowInfo.z;
    return filterShadow(directional, index, coord, texel);
}

// Visibility in [0, 1] of light at world position P.
float lightShadow(GpuLight light, vec3 P, vec3 N) {
    int base = int(light.shadowParams.y);
    if (base < 0) return 1.0;
    float type = light.colorType.w;
    if (type == LIGHT_DIRECTIONAL) {
        int cascadeCount = int(shadowInfo.x);
        float viewDepth = -(view * vec4(P, 1.0)).z;
        for (int c = 0; c < cascadeCount; c++) {
            if (viewDepth < cascadeSplits[c]) {
                return sampleShadowMatrix(true, base + c, P, N, 0.0, light);
            }
        }
        return 1.0;
    }
    vec3 fromLight = P - light.positionRange.xyz;
    int index = base;
    if (type == LIGHT_POINT) index += selectCubeFace(fromLight);
    return sampleShadowMatrix(false, index, P, N, length(fromLight), light);
}
