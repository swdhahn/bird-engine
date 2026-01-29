#version 430

layout(location = 0) in vec2 fragTexCoord;
layout(binding = 0) uniform sampler2D screenTexture;
layout(location = 0) out vec4 FragColor;

// ==========================================
//           COLOR GRADING SETTINGS
// ==========================================

// --- 1. INPUT TRANSFORM ---
#define EXPOSURE       1.0
#define SATURATION     1.1
#define CONTRAST       1.05

// --- 2. WHITE BALANCE (Multiplier) ---
// Warm: vec3(1.1, 1.0, 0.9) | Cool: vec3(0.9, 1.0, 1.1)
#define COLOR_FILTER   vec3(1.0, 1.0, 1.0)

// --- 3. 3-WAY CORRECTION (Lift, Gamma, Gain) ---
// Lift:  Shadow color offset (e.g. 0.0, 0.0, 0.02 for blue shadows)
// Gamma: Midtone power (1.0 = neutral)
// Gain:  Highlight multiplier (e.g. 1.1, 1.0, 0.9 for warm highlights)
#define LIFT           vec3(0.00, 0.00, 0.00)
#define GAMMA          vec3(1.00, 1.00, 1.00)
#define GAIN           vec3(1.00, 1.00, 1.00)

// --- 4. FINISHING ---
#define VIGNETTE_STR   0.4
#define VIGNETTE_RND   0.25
#define USE_ACES       1   // 1 = ON, 0 = OFF

// ==========================================

// Helper: Calculate Luminance (Perceived brightness)
float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// Helper: ACES Tone Mapping
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    // Sample the scene
    vec3 color = texture(screenTexture, fragTexCoord).rgb;

    // --- STEP 1: Basic Exposure & Temperature ---
    color *= EXPOSURE;
    color *= COLOR_FILTER;

    // --- STEP 2: Contrast & Saturation ---
    float luma = luminance(color);
    color = mix(vec3(luma), color, SATURATION);

    // Apply contrast around the midpoint (0.5)
    color = (color - 0.5) * CONTRAST + 0.5;
    color = max(vec3(0.0), color);

    // --- STEP 3: 3-Way Color Grading ---
    // 1. Lift
    color = color + LIFT * (1.0 - luma);
    // 2. Gain
    color = color * GAIN;
    // 3. Gamma
    color = max(vec3(0.0), color);
    color = pow(color, 1.0 / GAMMA);

    // --- STEP 4: Tone Mapping ---
    #if USE_ACES == 1
    color = ACESFilm(color);
    #else
    color = clamp(color, 0.0, 1.0);
    #endif

    // --- STEP 5: Vignette ---
    vec2 uv = fragTexCoord * (1.0 - fragTexCoord.yx);
    float vig = uv.x * uv.y * 15.0;
    vig = pow(vig, VIGNETTE_RND);
    color *= mix(1.0, vig, VIGNETTE_STR);

    // --- STEP 6: Dithering ---
    float dither = fract(sin(dot(fragTexCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    color += (dither - 0.5) / 255.0;

    FragColor = vec4(color, 1.0);
}
