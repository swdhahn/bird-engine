#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) flat in vec3 fragViewPos;

layout(binding = 0) uniform sampler2D tex;

layout(std140, binding = 1) uniform Material {
    vec4 diffuse;
    vec3 specular;
    vec3 ambient;
    float shininess;
};

layout(location = 0) out vec4 out_color;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(fragViewPos - fragPos);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 halfway = normalize(lightDir + viewDir);

    float lightIntensity = 1.0;
    vec3 lightColor = vec3(1.0);
    vec3 specColor = vec3(1.0);

    vec3 baseColor = mix(texture(tex, fragTexCoord).rgb, diffuse.xyz, diffuse.w);

    // Base Lighting
    float diffIntensity = 0.4 + lightIntensity * max(dot(normal, lightDir), 0.0);
    vec3 diffuseRes = diffIntensity * baseColor;

    // Specular Lighting
    float specFactor = pow(max(dot(normal, halfway), 0.0), 4.0); //shininess);
    vec3 specularRes = specFactor * vec3(0.1) * 0.5;

    out_color = vec4(diffuseRes + specularRes, 1.0);
}
