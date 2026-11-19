// Light list; mirrors bird::GpuLight / LightsUBOData (binding 2).
struct GpuLight {
    vec4 positionRange;      // xyz position, w range
    vec4 colorType;          // rgb radiance, w type
    vec4 directionCosOuter;  // xyz direction, w cos(outer)
    vec4 shadowParams;       // x cos(inner), y shadow index, z depth bias, w normal bias
};

const float LIGHT_DIRECTIONAL = 0.0;
const float LIGHT_POINT = 1.0;
const float LIGHT_SPOT = 2.0;

// Inverse-square falloff windowed to reach zero at range (Karis 2013).
float distanceAttenuation(float distanceSquared, float range) {
    float factor = distanceSquared / (range * range);
    float window = clamp(1.0 - factor * factor, 0.0, 1.0);
    return window * window / max(distanceSquared, 1e-4);
}

float spotAttenuation(vec3 L, vec3 spotDirection, float cosOuter,
                      float cosInner) {
    float cd = dot(-L, spotDirection);
    float t = clamp((cd - cosOuter) / max(cosInner - cosOuter, 1e-4), 0.0, 1.0);
    return t * t;
}

// Returns the unit vector toward the light and the radiance reaching P,
// excluding shadowing.
vec3 incidentRadiance(GpuLight light, vec3 P, out vec3 L) {
    float type = light.colorType.w;
    if (type == LIGHT_DIRECTIONAL) {
        L = -light.directionCosOuter.xyz;
        return light.colorType.rgb;
    }
    vec3 toLight = light.positionRange.xyz - P;
    float distanceSquared = dot(toLight, toLight);
    L = toLight * inversesqrt(max(distanceSquared, 1e-8));
    float attenuation =
        distanceAttenuation(distanceSquared, light.positionRange.w);
    if (type == LIGHT_SPOT) {
        attenuation *= spotAttenuation(L, light.directionCosOuter.xyz,
                                       light.directionCosOuter.w,
                                       light.shadowParams.x);
    }
    return light.colorType.rgb * attenuation;
}
