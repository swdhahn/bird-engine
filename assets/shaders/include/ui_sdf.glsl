// Shared UI shading, included by ui.frag. Edits here re-bake every shader
// that includes this file (BuildTools tracks #include dependencies).

// Atlas texels spanned by the full 0..1 distance range. Must equal
// 255 / kFontSdfPixelDistScale in AssetLoader.cpp (255 / (128 / 6)).
const float SDF_TEXEL_RANGE = 255.0 / (128.0 / 6.0);

// Signed distance from p to a box of half size `halfSize` with rounded
// corners of radius r (negative inside). p is relative to the box centre.
float roundedBoxDistance(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// Applies a rounded-box shape to an already shaded colour:
// shape.xy = position from the box centre, shape.zw = half size (0 = none);
// style.x = radius, style.y = border width, style.z = softness (blur px).
vec4 applyShape(vec4 shaded, vec4 shape, vec4 style, vec4 borderColor) {
    if (shape.z <= 0.0) {
        return shaded;
    }
    float d = roundedBoxDistance(shape.xy, shape.zw, style.x);
    // 1px anti-aliasing ramp, or a wide ramp for blurred shadows.
    float softness = max(style.z, 0.5);
    float coverage = 1.0 - smoothstep(-softness, softness, d);
    vec4 color = shaded;
    if (style.y > 0.0) {
        float band = smoothstep(-style.y - 0.5, -style.y + 0.5, d);
        color = mix(shaded, borderColor, band);
    }
    return vec4(color.rgb, color.a * coverage);
}

// Font atlas: white RGB with a signed distance field in alpha (0.5 on the
// outline). Solid quads sample a texel deep inside (1.0), so one path
// serves both text and panels.
vec4 uiShade(vec4 color, sampler2D atlas, vec2 uv) {
    vec4 texel = texture(atlas, uv);
    // Screen pixels covered by the distance range. UV derivatives are
    // constant across a quad, so the ramp stays stable at small sizes.
    vec2 texelsPerPixel = fwidth(uv) * vec2(textureSize(atlas, 0));
    float screenRange = max(SDF_TEXEL_RANGE / max(0.5 * (texelsPerPixel.x + texelsPerPixel.y), 1e-4), 1.0);
    float coverage = clamp(screenRange * (texel.a - 0.5) + 0.5, 0.0, 1.0);
    return vec4(color.rgb * texel.rgb, color.a * coverage);
}
