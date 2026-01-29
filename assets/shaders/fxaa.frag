#version 430

layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D screenTexture;

layout(location = 0) out vec4 FragColor;

// FXAA Settings
// FXAA_SPAN_MAX: The maximum distance to search for an edge end.
// FXAA_REDUCE_MIN: Bias to avoid division by zero.
// FXAA_REDUCE_MUL: Controls how much the algorithm reduces AA on low contrast areas.
#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL   (1.0/8.0)
#define FXAA_REDUCE_MIN   (1.0/128.0)

void main()
{
    // 1. Calculate the size of a single texel automatically
    // textureSize is available in version 130+, so it works great in 430
    ivec2 size = textureSize(screenTexture, 0);
    vec2 inverseScreenSize = 1.0 / vec2(size);

    vec3 rgbM = texture(screenTexture, fragTexCoord).xyz;

    // 2. Sampling neighbor pixels (NorthWest, NorthEast, SouthWest, SouthEast)
    vec3 rgbNW = texture(screenTexture, fragTexCoord + (vec2(-1.0, -1.0) * inverseScreenSize)).xyz;
    vec3 rgbNE = texture(screenTexture, fragTexCoord + (vec2(1.0, -1.0) * inverseScreenSize)).xyz;
    vec3 rgbSW = texture(screenTexture, fragTexCoord + (vec2(-1.0, 1.0) * inverseScreenSize)).xyz;
    vec3 rgbSE = texture(screenTexture, fragTexCoord + (vec2(1.0, 1.0) * inverseScreenSize)).xyz;

    // 3. Convert to Luma (Grayscale) for edge detection
    // Luma weights based on human perception
    vec3 luma = vec3(0.299, 0.587, 0.114);

    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    // 4. Determine local contrast range
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // 5. Calculate gradient direction
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    // 6. Scale direction to reduce artifacts on low contrast areas
    float dirReduce = max(
            (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
            FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
            max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
                dir * rcpDirMin)) * inverseScreenSize;

    // 7. Perform the blur samples along the calculated direction
    vec3 rgbA = (1.0 / 2.0) * (
            texture(screenTexture, fragTexCoord.xy + dir * (1.0 / 3.0 - 0.5)).xyz +
                texture(screenTexture, fragTexCoord.xy + dir * (2.0 / 3.0 - 0.5)).xyz);

    vec3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (
                texture(screenTexture, fragTexCoord.xy + dir * (0.0 / 3.0 - 0.5)).xyz +
                    texture(screenTexture, fragTexCoord.xy + dir * (3.0 / 3.0 - 0.5)).xyz);

    float lumaB = dot(rgbB, luma);

    // 8. Final selection: if the heavily blurred version (B) goes outside the
    // local contrast range, it's wrong—use the lighter blur (A).
    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        FragColor = vec4(rgbA, 1.0);
    } else {
        FragColor = vec4(rgbB, 1.0);
    }
}
