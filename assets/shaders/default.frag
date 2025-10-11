#version 430

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) flat in vec3 fragViewPos;

layout(binding = 0) uniform sampler2D tex;

layout(std140, binding = 1) uniform Material {
    vec3 diffuse;
    vec3 specular;
    vec3 ambient;
    float shininess;
};

layout(location = 0) out vec4 out_color;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    vec3 viewDir = normalize(fragViewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, fragNormal);
    vec3 spec = lightColor /*specColor*/ * pow(max(dot(viewDir, reflectDir), 0.0), 32 /*shininess*/ ) * 0.5;

    float lighting = 0.2 + max(0.0, dot(fragNormal, lightDir)) * 0.8;
    vec3 color = (lighting + spec) * (diffuse + texture(tex, fragTexCoord).rgb);
    out_color = vec4(color, 1.0);
}
