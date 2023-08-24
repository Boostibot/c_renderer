#version 330 core
out vec4 frag_color;

uniform vec3 color;

void main()
{
    float originalZ = gl_FragCoord.z / gl_FragCoord.w;
    vec3 mixed_color = color * 1/originalZ;
    frag_color = vec4(mixed_color, 1.0);
}