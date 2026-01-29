#version 430

layout(location = 0) in vec2 fragTexCoord;

// Binding 0: The Color Image
layout(binding = 0) uniform sampler2D screenTexture;

// Binding 1: The Depth Map (REQUIRED for SSAO)
layout(binding = 1) uniform sampler2D depthTexture;

layout(location = 0) out vec4 FragColor;

// ==========================================
//              SSAO SETTINGS
// ==========================================

// Quality (Higher = slower but smoother)
#define KERNEL_SIZE  64

// Radius: How wide the shadow spreads (in world units)
#define RADIUS       0.1

// Bias: Prevents "acne" (self-shadowing artifacts)
#define BIAS         0.025

// Strength: How dark the shadows are (1.0 = standard)
#define INTENSITY    1.5

// Camera Settings (MUST MATCH YOUR C++ CAMERA)
// Since we can't use uniforms, we hardcode these to reconstruct position.
#define FOV          45.0
#define NEAR_PLANE   0.1
#define FAR_PLANE    100.0

// ==========================================

// Helper: Pseudo-random number generator
// Generates noise without a texture
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// Helper: Reconstruct View-Space Position from Depth
// We reverse the projection matrix math here.
vec3 getViewPos(vec2 texCoord) {
    // 1. Sample raw depth (non-linear usually 0..1)
    float z_b = texture(depthTexture, texCoord).r;

    // 2. Convert to Normalized Device Coordinates (NDC) [-1, 1]
    // If you use OpenGL 4.5+ clip control, this might differ.
    // This assumes standard glDepthRange(0,1)
    float z_ndc = z_b * 2.0 - 1.0;

    // 3. Convert NDC z to View Z (Linearize depth)
    // Formula: (2.0 * n * f) / (f + n - z_ndc * (f - n))
    float viewZ = (2.0 * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z_ndc * (FAR_PLANE - NEAR_PLANE));

    // 4. Calculate View X and Y
    // We need the screen dimensions to know the aspect ratio
    ivec2 screenDim = textureSize(screenTexture, 0);
    float aspect = float(screenDim.x) / float(screenDim.y);

    // Calculate the size of the view frustum at this depth
    float tanHalfFov = tan(radians(FOV) / 2.0);

    // Convert uv (0..1) to ndc xy (-1..1)
    vec2 clipXY = texCoord * 2.0 - 1.0;

    float viewX = clipXY.x * aspect * tanHalfFov * viewZ; // viewZ is negative in OpenGL view space?
    // Actually standard reconstruction usually assumes positive depth distance for simplicity here,
    // but strictly OpenGL view is -Z. Let's keep it simple: magnitude of Z.
    // If we treat Z as a distance:
    viewX = clipXY.x * aspect * tanHalfFov * viewZ;
    float viewY = clipXY.y * tanHalfFov * viewZ;

    return vec3(viewX, viewY, -viewZ); // -Z for OpenGL View Space
}

void main()
{
    // 1. Reconstruct Position and Normal
    vec3 fragPos = getViewPos(fragTexCoord);

    // We calculate normal using derivatives (flat shading appearance)
    // This saves us from needing a Normal G-Buffer texture.
    vec3 normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));

    // 2. Create a random rotation vector for the kernel
    // We use screen position to vary the noise per pixel
    vec2 noiseScale = vec2(textureSize(screenTexture, 0)) / 4.0;
    vec3 randomVec = normalize(vec3(rand(fragTexCoord * noiseScale), rand(fragTexCoord), 0.0));

    // 3. Create TBN Matrix (Tangent-Bitangent-Normal)
    // This allows us to orient our sample kernel along the surface normal
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    // 4. Aspect Ratio & Projection Constants (Pre-calc for loop)
    ivec2 screenDim = textureSize(screenTexture, 0);
    float aspect = float(screenDim.x) / float(screenDim.y);
    float tanHalfFov = tan(radians(FOV) / 2.0);

    // 5. Iterate over the kernel
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // Generate a sample position in a hemisphere
        // (Using simple math instead of a uniform array)
        vec3 samplePos;

        // Use loop index as seed for randomness to get different sample per iteration
        float t = float(i) / float(KERNEL_SIZE);
        float r1 = rand(vec2(t, fragTexCoord.x));
        float r2 = rand(vec2(t, fragTexCoord.y));

        // Hemisphere sampling logic
        float theta = 2.0 * 3.14159 * r1;
        float phi = acos(1.0 - 2.0 * r2) * 0.5; // Half sphere

        samplePos = vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
        // Distribute samples closer to center (accelerating interpolation)
        samplePos *= mix(0.1, 1.0, r1 * r1);

        // Transform sample from Tangent Space to View Space
        samplePos = fragPos + (TBN * samplePos) * RADIUS;

        // 6. Project sample position to Screen Space (to find texture coordinate)
        vec4 offset = vec4(samplePos, 1.0);

        // Manual Perspective Projection Matrix Multiplication
        // P[0][0] = 1 / (aspect * tan), P[1][1] = 1 / tan, etc.
        // We do this manually to avoid needing a Uniform Matrix.
        offset.x = (offset.x / (aspect * tanHalfFov));
        offset.y = (offset.y / (tanHalfFov));
        offset.z = -offset.z; // Invert Z for projection division

        // Perspective divide
        offset.xy /= offset.z; // This is now in NDC -1..1

        // Transform NDC to 0..1 Texture Coordinates
        offset.xy = offset.xy * 0.5 + 0.5;

        // 7. Sample depth at that spot
        // If the sample is off-screen, clamp it or it will wrap around
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) continue;

        // Get the depth of the geometry at that sample point
        float sampleDepth = getViewPos(offset.xy).z; // Returns negative Z (e.g. -10)

        // Check occlusion
        // fragPos.z is e.g. -5.0. sampleDepth is -4.9.
        // We check: is sampleDepth > samplePos.z + bias?
        // Working with magnitudes is easier:
        // Actual geometry depth magnitude vs Sample point depth magnitude
        float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(fragPos.z - sampleDepth));

        // Note: sampleDepth is negative. samplePos.z is negative.
        // If sampleDepth >= samplePos.z, it means the geometry is BEHIND the sample point.
        // We want to know if geometry is IN FRONT OF the sample point.
        // Geometry (-4) > Sample (-5).
        if (sampleDepth >= samplePos.z + BIAS)
        {
            occlusion += 1.0 * rangeCheck;
        }
    }

    occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
    occlusion = pow(occlusion, INTENSITY); // Intensify

    vec3 sceneColor = texture(screenTexture, fragTexCoord).rgb;
    FragColor = vec4(sceneColor * occlusion, 1.0);
}
