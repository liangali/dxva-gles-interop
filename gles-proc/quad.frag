#version 300 es
precision mediump float;

uniform vec3 u_RVec3;
uniform vec3 u_GVec3;
uniform vec3 u_BVec3;

in vec2 texcoord;
out vec3 fragcolor;
uniform sampler2D tex;

void main()
{
    vec4 oriColor = texture(tex, texcoord);
    vec4 resColor = vec4(0.0);
    resColor.r = dot(oriColor.rgb, u_RVec3);
    resColor.g = dot(oriColor.rgb, u_GVec3);
    resColor.b = dot(oriColor.rgb, u_BVec3);
    fragcolor = resColor.rgb; //vec3(0.5, 1.0, 0.2);
}
