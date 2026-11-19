// Metallic-roughness parameters; mirrors bird::MaterialUBOData (binding 1).
layout(std140, binding = 1) uniform Material {
    vec4 baseColor;
    vec4 emissive;           // rgb color, w strength
    float metallic;
    float roughness;
    float alphaCutoff;
    float normalScale;
    float occlusionStrength;
    uint textureMask;
    uint flags;
    float materialPadding0;
};

layout(binding = 0) uniform sampler2D baseColorMap;
layout(binding = 1) uniform sampler2D metallicRoughnessMap;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D occlusionMap;
layout(binding = 4) uniform sampler2D emissiveMap;

const uint TEX_BASE_COLOR = 1u;
const uint TEX_METALLIC_ROUGHNESS = 2u;
const uint TEX_NORMAL = 4u;
const uint TEX_OCCLUSION = 8u;
const uint TEX_EMISSIVE = 16u;
const uint FLAG_ALPHA_TEST = 1u;
const uint FLAG_UNLIT = 2u;

struct SurfaceSample {
    vec4 albedo;      // linear rgb, alpha
    vec3 normal;      // world space, normalized
    float roughness;
    float metallic;
    float occlusion;
    vec3 emissive;
};

// Derivative-based tangent frame for meshes baked without tangents
// (Schueler, "Normal Mapping without Precomputed Tangents").
mat3 cotangentFrame(vec3 n, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(t, t), dot(b, b)) + 1e-20);
    return mat3(t * invmax, b * invmax, n);
}

// Evaluates every material input for one fragment.
SurfaceSample sampleSurface(vec3 worldPos, vec3 vertexNormal, vec4 tangent,
                            vec2 uv, bool frontFacing) {
    SurfaceSample s;
    s.albedo = baseColor;
    if ((textureMask & TEX_BASE_COLOR) != 0u) {
        s.albedo *= texture(baseColorMap, uv);
    }

    vec3 n = normalize(vertexNormal);
    if (!frontFacing) n = -n;
    if ((textureMask & TEX_NORMAL) != 0u) {
        vec3 tn = texture(normalMap, uv).xyz * 2.0 - 1.0;
        tn.xy *= normalScale;
        mat3 tbn;
        if (dot(tangent.xyz, tangent.xyz) > 0.25) {
            vec3 t = normalize(tangent.xyz - n * dot(n, tangent.xyz));
            vec3 b = cross(n, t) * (tangent.w < 0.0 ? -1.0 : 1.0);
            tbn = mat3(t, b, n);
        } else {
            tbn = cotangentFrame(n, worldPos, uv);
        }
        n = normalize(tbn * tn);
    }
    s.normal = n;

    s.roughness = roughness;
    s.metallic = metallic;
    if ((textureMask & TEX_METALLIC_ROUGHNESS) != 0u) {
        // glTF packing: roughness in G, metalness in B.
        vec3 mr = texture(metallicRoughnessMap, uv).rgb;
        s.roughness *= mr.g;
        s.metallic *= mr.b;
    }
    s.roughness = clamp(s.roughness, 0.02, 1.0);
    s.metallic = clamp(s.metallic, 0.0, 1.0);

    s.occlusion = 1.0;
    if ((textureMask & TEX_OCCLUSION) != 0u) {
        s.occlusion = mix(1.0, texture(occlusionMap, uv).r, occlusionStrength);
    }

    s.emissive = emissive.rgb * emissive.w;
    if ((textureMask & TEX_EMISSIVE) != 0u) {
        s.emissive *= texture(emissiveMap, uv).rgb;
    }
    return s;
}
