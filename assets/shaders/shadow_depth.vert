#version 450

layout(location = 0) in vec3 position;

layout(push_constant) uniform ShadowConstants {
    mat4 model;
    mat4 lightViewProjection;
} caster;

void main() {
    gl_Position = caster.lightViewProjection * caster.model *
                  vec4(position, 1.0);
}
