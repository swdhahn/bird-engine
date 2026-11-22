#version 450

// Converts an equirectangular HDR environment map into a cubemap face.
layout(location = 0) in vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uEquirectangularMap;

const vec2 INV_ATAN = vec2(0.15915494309, 0.31830988618);  // 1/(2*pi), 1/pi

vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= INV_ATAN;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = sampleSphericalMap(normalize(localPos));
    vec3 color = texture(uEquirectangularMap, uv).rgb;
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
