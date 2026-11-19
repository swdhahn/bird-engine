// Camera and frame constants; mirrors bird::GlobalUBOData (binding 0).
layout(std140, binding = 0) uniform Global {
    mat4 perspective;
    mat4 view;
    vec4 viewPos;
    mat4 inverseProjection;
    mat4 inverseView;
    vec4 screenSize;          // w, h, 1/w, 1/h
    vec4 ambientAndExposure;  // rgb ambient * intensity, w exposure
    vec4 cameraParams;        // near, far, debug view, unused
};
