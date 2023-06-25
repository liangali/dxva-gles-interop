#version 300 es

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_texcoord;

out vec2 texcoord;

void main()
{
    texcoord = in_texcoord;
    gl_Position = vec4(in_position, 1.0);
}
