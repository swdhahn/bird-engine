#version 450

// Generates a physically-inspired procedural HDR sky cubemap (sky gradient,
// Mie-like sun halo, crisp sun disc, and ground hemisphere bounce).
layout(location = 0) in vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 1) uniform ProceduralSky {
    vec4 sunDirAndCosRadius;    // xyz = dir toward sun, w = cos(angularRadius)
    vec4 sunColorAndIntensity;  // rgb = sun color, w = intensity
    vec4 zenithColor;
    vec4 horizonColor;
    vec4 groundColor;
};

void main() {
    vec3 dir = normalize(localPos);
    float elevation = dir.y;

    // Sky hemisphere gradient (horizon -> zenith) vs. ground hemisphere bounce.
    vec3 color;
    if (elevation >= 0.0) {
        float t = pow(clamp(elevation, 0.0, 1.0), 0.55);
        color = mix(horizonColor.rgb, zenithColor.rgb, t);
    } else {
        float t = pow(clamp(-elevation, 0.0, 1.0), 0.45);
        color = mix(horizonColor.rgb * 0.75, groundColor.rgb, t);
    }

    // Subtle atmospheric forward-scatter halo around the sun.
    vec3 sunDir = normalize(sunDirAndCosRadius.xyz);
    float sunDot = max(dot(dir, sunDir), 0.0);
    float halo = pow(sunDot, 16.0) * 0.35 + pow(sunDot, 128.0) * 1.25;
    color += sunColorAndIntensity.rgb * (halo * sunColorAndIntensity.w * 0.12);

    // Crisp sun disc above the horizon.
    float cosRadius = sunDirAndCosRadius.w;
    float disc = smoothstep(cosRadius - 0.0015, cosRadius + 0.0005, sunDot);
    float horizonMask = smoothstep(-0.02, 0.05, elevation);
    color += sunColorAndIntensity.rgb * (disc * sunColorAndIntensity.w * horizonMask);

    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
