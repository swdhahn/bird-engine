#version 450

// Convolves an environment cubemap over the positive hemisphere using cosine
// weighting to produce the diffuse irradiance cubemap.
layout(location = 0) in vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform samplerCube uEnvironmentMap;

const float PI = 3.14159265359;

void main() {
    vec3 N = normalize(localPos);
    // Explicit branch, as in ibl_prefilter.frag: the ternary form selected
    // the parallel axis for N = +/-Y and produced NaN samples.
    vec3 up = vec3(0.0, 0.0, 1.0);
    if (abs(N.y) < 0.999) {
        up = vec3(0.0, 1.0, 0.0);
    }
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        float cosPhi = cos(phi);
        float sinPhi = sin(phi);
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);
            vec3 tangentSample = vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            vec3 sampleVec = tangentSample.x * right +
                             tangentSample.y * up +
                             tangentSample.z * N;
            irradiance += texture(uEnvironmentMap, sampleVec).rgb * cosTheta * sinTheta;
            nrSamples += 1.0;
        }
    }

    irradiance = PI * irradiance * (1.0 / max(nrSamples, 1.0));
    outColor = vec4(irradiance, 1.0);
}
