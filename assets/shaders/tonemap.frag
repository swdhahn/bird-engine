#version 450

// HDR resolve: exposure, ACES filmic curve, sRGB encoding, dither.
// Also renders G-buffer debug views selected by cameraParams.z.
layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

#include "include/globals.glsl"
#include "include/gbuffer.glsl"

layout(binding = 0) uniform sampler2D hdrColor;
layout(binding = 5) uniform sampler2D gAlbedoOcclusion;
layout(binding = 6) uniform sampler2D gNormalRoughnessMetallic;
layout(binding = 7) uniform sampler2D gDepth;

// Narkowicz 2015 fit of the ACES reference rendering transform.
vec3 acesFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 linearToSrgb(vec3 c) {
    vec3 low = c * 12.92;
    vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, vec3(lessThanEqual(c, vec3(0.0031308))));
}

// Interleaved gradient noise removes banding in dark gradients.
float dither(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

void main() {
    int mode = int(cameraParams.z);
    vec3 color;
    if (mode == 1) {
        color = linearToSrgb(texture(gAlbedoOcclusion, fragTexCoord).rgb);
    } else if (mode == 2) {
        color = decodeNormal(texture(gNormalRoughnessMetallic, fragTexCoord).xy) *
                    0.5 + 0.5;
    } else if (mode == 3) {
        color = vec3(texture(gNormalRoughnessMetallic, fragTexCoord).zw, 0.0);
    } else if (mode == 4) {
        float d = texture(gDepth, fragTexCoord).r * 2.0 - 1.0;
        float n = cameraParams.x, f = cameraParams.y;
        float linear = (2.0 * n * f) / (f + n - d * (f - n));
        color = vec3(clamp(linear / 100.0, 0.0, 1.0));
    } else {
        vec3 hdr = texture(hdrColor, fragTexCoord).rgb * ambientAndExposure.w;
        color = linearToSrgb(acesFilm(hdr));
    }
    color += (dither(gl_FragCoord.xy) - 0.5) / 255.0;
    outColor = vec4(color, 1.0);
}
