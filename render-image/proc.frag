#version 300 es
precision mediump float;

in vec2 texcoord;
out vec3 fragcolor;
uniform sampler2D tex;

vec2 Flip_v(float flip, vec2 uv);
vec3 Flip_v(float flip, vec3 uv);
vec4 Flip_v(float flip, vec4 uv);

vec2 Flip_v(float flip, vec2 uv) {
    if(flip > 0.5)
        uv.y = 1.0 - uv.y;
    return uv;
}
vec3 Flip_v(float flip, vec3 uv) {
    if(flip > 0.5)
        uv.y = 1.0 - uv.y;
    return uv;
}
vec4 Flip_v(float flip, vec4 uv) {
    if(flip > 0.5)
        uv.y = 1.0 - uv.y;
    return uv;
}

void main()
{
    fragcolor = texture(tex, Flip_v(1.0,texcoord)).rgb;
    fragcolor = fragcolor * min(1.0, 1.0);
}
