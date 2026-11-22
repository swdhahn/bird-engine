#version 450

// Renders a unit cube into one face of a cubemap using a 90-degree capture
// view-projection matrix. Passes object-space direction to the fragment stage.
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 localPos;

layout(push_constant) uniform IBLCubemap {
    mat4 viewProjection;
} pc;

void main() {
    localPos = inPosition;
    gl_Position = pc.viewProjection * vec4(inPosition, 1.0);
}
