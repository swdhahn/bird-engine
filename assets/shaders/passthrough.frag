#version 430

layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D screenTexture;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = texture(screenTexture, fragTexCoord);
}
