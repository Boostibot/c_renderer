#version 330 core
out vec4 frag_color;

uniform vec3 color;
uniform float gamma;

vec3 gamma_correct(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

void main()
{
    frag_color = vec4(gamma_correct(color, 1.0/gamma), 1.0);
}