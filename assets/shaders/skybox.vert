#version 450

// Skybox vertex shader: strips translation from the camera view matrix and
// forces NDC depth to 1.0 (xyww) so the skybox renders behind all geometry
// when tested with GL_LEQUAL.
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 sampleDir;

#include "include/globals.glsl"

void main() {
    sampleDir = inPosition;
    mat4 rotView = mat4(mat3(view));
    vec4 clipPos = perspective * rotView * vec4(inPosition, 1.0);
    gl_Position = clipPos.xyww;
}
