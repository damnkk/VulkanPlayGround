#version 460

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(location = 0) in vec2 outUV;

layout(location = 0) out vec4 fragColor ;
void main(){
    fragColor = texture(inputTexture,outUV);
}