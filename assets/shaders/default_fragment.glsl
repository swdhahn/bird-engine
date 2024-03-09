#version 430

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(binding = 0) uniform sampler2D tex;

/*layout(std140, binding = 0) uniform Material {
    vec3 diffuse;
    vec3 ambient;
    vec3 specular;
    float shininess;
} material;*/

/*layout(std140, binding = 1) uniform Light {
    vec3 position;
    vec3 color;
} light;*/

/*layout(std140, binding = 2) uniform Camera {
    mat4 view;
    mat4 projection;
    vec3 position;
} camera;*/

/*layout(std140, binding = 3) uniform Transform {
    mat4 model;
    mat4 normal;
} transform;*/

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(0.5, 0.5, 0.5, 1.0);//vec4(texture(tex, fragTexCoord * 1).rgb, 1.0);
}

