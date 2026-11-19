#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 clipRect;	// minX, minY, maxX, maxY (window px)

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out vec4 fragClip;
layout(location = 3) out vec2 fragPosition;

layout(std140, binding = 0) uniform Global {
    mat4 perspective;
    mat4 view;
    vec4 viewPos;
};

void main()
{
    gl_Position = perspective * vec4(position, 0.0, 1.0);
    fragColor = color;
    fragTexCoord = texCoord;
    fragClip = clipRect;
    fragPosition = position;
}
