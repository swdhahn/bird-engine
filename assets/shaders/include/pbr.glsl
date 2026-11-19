// Cook-Torrance microfacet BRDF: GGX distribution, height-correlated Smith
// visibility, Schlick Fresnel, Lambertian diffuse (Filament conventions).
const float PI = 3.14159265359;

float distributionGGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f + 1e-7);
}

float visibilitySmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float ggxL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float ggxV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    return 0.5 / (ggxV + ggxL + 1e-7);
}

vec3 fresnelSchlick(float VoH, vec3 f0) {
    float f = pow(1.0 - VoH, 5.0);
    return f0 + (vec3(1.0) - f0) * f;
}

// Outgoing radiance scale for unit incident radiance from direction L,
// already multiplied by N.L.
vec3 evaluateBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic,
                  float perceptualRoughness) {
    vec3 H = normalize(V + L);
    float NoV = abs(dot(N, V)) + 1e-5;
    float NoL = clamp(dot(N, L), 0.0, 1.0);
    float NoH = clamp(dot(N, H), 0.0, 1.0);
    float VoH = clamp(dot(V, H), 0.0, 1.0);
    if (NoL <= 0.0) return vec3(0.0);

    float a = perceptualRoughness * perceptualRoughness;
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(VoH, f0);
    vec3 specular =
        F * (distributionGGX(NoH, a) * visibilitySmithGGXCorrelated(NoV, NoL, a));
    vec3 diffuse = (1.0 - metallic) * albedo / PI;
    return (diffuse * (vec3(1.0) - F) + specular) * NoL;
}

// Ambient approximation: diffuse irradiance plus a Fresnel-weighted
// specular term, both scaled by occlusion.
vec3 evaluateAmbient(vec3 N, vec3 V, vec3 albedo, float metallic,
                     float perceptualRoughness, float occlusion,
                     vec3 ambient) {
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = f0 + (max(vec3(1.0 - perceptualRoughness), f0) - f0) *
                      pow(1.0 - NoV, 5.0);
    vec3 diffuse = (1.0 - metallic) * albedo * (vec3(1.0) - F);
    return (diffuse + F * 0.5) * ambient * occlusion;
}
