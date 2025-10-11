#version 430

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(binding = 0) uniform sampler2D tex;

layout(std140, binding = 1) uniform Material {
    vec3 diffuse;
    vec3 specular;
    vec3 ambient;
    float shininess;
};

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(max(vec3(1.0), diffuse), 1.0); //vec4(texture(tex, fragTexCoord * 1).rgb, 1.0);
}
