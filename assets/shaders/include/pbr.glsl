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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 f0, float roughness) {
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) *
                    pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Ambient approximation used when no IBL environment map is bound:
// diffuse irradiance plus a Fresnel-weighted specular term, scaled by occlusion.
vec3 evaluateAmbient(vec3 N, vec3 V, vec3 albedo, float metallic,
                     float perceptualRoughness, float occlusion,
                     vec3 ambient) {
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlickRoughness(NoV, f0, perceptualRoughness);
    vec3 diffuse = (1.0 - metallic) * albedo * (vec3(1.0) - F);
    return (diffuse + F * 0.5) * ambient * occlusion;
}

// Image-Based Lighting (split-sum approximation): combines diffuse irradiance
// with roughness-prefiltered specular environment reflections and the 2D BRDF
// integration LUT so metallic and dielectric surfaces reflect the environment.
vec3 evaluateIBL(vec3 N, vec3 V, vec3 albedo, float metallic,
                 float perceptualRoughness, float occlusion,
                 vec3 irradiance, vec3 prefilteredColor, vec2 brdf,
                 float iblIntensity) {
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlickRoughness(NoV, f0, perceptualRoughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = irradiance * albedo * kD;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    return (diffuse + specular) * occlusion * iblIntensity;
}
