#version 430

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 out_color;

// Font atlas: white RGB with coverage in alpha. Solid quads sample a fully
// covered texel, so a single path serves both text and panels.
layout(binding = 0) uniform sampler2D atlas;

void main() {
    out_color = vec4(fragColor, 1.0) * texture(atlas, fragTexCoord);
}
