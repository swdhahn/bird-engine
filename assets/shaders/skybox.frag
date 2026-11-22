#version 450

// Skybox fragment shader: samples the linear HDR environment cubemap directly
// into the HDR target (tone mapping and FXAA follow in post-processing).
layout(location = 0) in vec3 sampleDir;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform samplerCube uSkybox;

void main() {
    vec3 color = texture(uSkybox, normalize(sampleDir)).rgb;
    outColor = vec4(color, 1.0);
}
