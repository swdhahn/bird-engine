#version 450

#include "include/skinning.glsl"

layout(location = 0) in vec3 position;

// Same block as shadow_depth.vert: the shadow pass fills both identically.
layout(push_constant) uniform ShadowConstants {
    mat4 model;
    mat4 lightViewProjection;
} caster;

void main() {
    gl_Position = caster.lightViewProjection * caster.model *
                  vec4(skinPoint(skinMatrix(), position), 1.0);
}
