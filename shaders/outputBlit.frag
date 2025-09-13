#version 460

layout(location = 0) out vec4 outColor;
layout(binding = 0, set = 0) uniform sampler2D texColor;
layout(location = 0) in vec2 texCoord;

void main()
{
    outColor = texture(texColor, texCoord); // White color
}