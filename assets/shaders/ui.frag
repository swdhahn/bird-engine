#version 450

#include "include/ui_sdf.glsl"

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in vec4 fragClip;
layout(location = 3) in vec2 fragPosition;
layout(location = 4) in vec4 fragShape;
layout(location = 5) flat in vec4 fragStyle;
layout(location = 6) flat in vec4 fragBorderColor;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D atlas;

void main() {
    // Clipping in the shader keeps the whole UI in one draw call.
    if (fragPosition.x < fragClip.x || fragPosition.y < fragClip.y ||
        fragPosition.x > fragClip.z || fragPosition.y > fragClip.w) {
        discard;
    }
    // Images sample their own texture directly; everything else shares
    // the font atlas, whose alpha holds a signed distance field.
    vec4 shaded = fragStyle.w > 0.5 ? fragColor * texture(atlas, fragTexCoord)
                                    : uiShade(fragColor, atlas, fragTexCoord);
    out_color = applyShape(shaded, fragShape, fragStyle, fragBorderColor);
}
