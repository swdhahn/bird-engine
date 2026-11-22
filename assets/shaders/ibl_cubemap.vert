#version 450

// Renders a unit cube into one face of a cubemap using a 90-degree capture
// view-projection matrix. Passes object-space direction to the fragment stage.
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 localPos;

// Shared with ibl_prefilter.frag: OpenGL links push constants as one
// uniform, so both stages must declare the identical block. Passes without
// a roughness parameter push only viewProjection.
layout(push_constant) uniform IBLCapture {
    mat4 viewProjection;
    vec4 params;  // x = roughness [0..1], y = env cubemap resolution
} pc;

void main() {
    localPos = inPosition;
    gl_Position = pc.viewProjection * vec4(inPosition, 1.0);
}
