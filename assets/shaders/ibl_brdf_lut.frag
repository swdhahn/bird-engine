#version 450

// Computes the 2D split-sum BRDF integration LUT (RG16F):
//   x = F0 scale, y = F0 bias
// Indexed by (NoV, perceptualRoughness) in [0, 1]^2.
layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radicalInverseVdC(i));
}

vec3 importanceSampleGGX(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    // An explicit branch: the ternary form cross-compiled to a select that
    // picked +Z for N = +Z, so cross(up, N) was zero and every sample NaN.
    vec3 up = vec3(1.0, 0.0, 0.0);
    if (abs(N.z) < 0.999) {
        up = vec3(0.0, 0.0, 1.0);
    }
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float geometrySchlickGGX_IBL(float NoV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return NoV / (NoV * (1.0 - k) + k + 1e-7);
}

float geometrySmithIBL(float NoV, float NoL, float roughness) {
    return geometrySchlickGGX_IBL(NoV, roughness) *
           geometrySchlickGGX_IBL(NoL, roughness);
}

vec2 integrateBRDF(float NoV, float roughness) {
    vec3 V = vec3(sqrt(max(1.0 - NoV * NoV, 0.0)), 0.0, NoV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;
    const uint SAMPLE_COUNT = 512u;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = hammersley(i, SAMPLE_COUNT);
        vec3 H = importanceSampleGGX(xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NoL = max(L.z, 0.0);
        float NoH = max(H.z, 0.0);
        float VoH = max(dot(V, H), 0.0);

        if (NoL > 0.0) {
            float G = geometrySmithIBL(NoV, NoL, roughness);
            float G_Vis = (G * VoH) / (NoH * NoV + 1e-5);
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}

void main() {
    float NoV = max(fragTexCoord.x, 1e-4);
    float roughness = clamp(fragTexCoord.y, 0.0, 1.0);
    vec2 integrated = integrateBRDF(NoV, roughness);
    outColor = vec4(integrated, 0.0, 1.0);
}
