// Shared UI shading, included by ui.frag. Edits here re-bake every shader
// that includes this file (BuildTools tracks #include dependencies).

// Atlas texels spanned by the full 0..1 distance range. Must equal
// 255 / kFontSdfPixelDistScale in AssetLoader.cpp (255 / (128 / 6)).
const float SDF_TEXEL_RANGE = 255.0 / (128.0 / 6.0);

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
