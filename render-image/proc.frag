#version 300 es
precision mediump float;

in vec2 texcoord;
out vec3 fragcolor;
uniform sampler2D tex;

void main()
{
    fragcolor = vec3( 1.0, 0.0, 0.0); //1.0 - texture(tex, texcoord).rgb;
}