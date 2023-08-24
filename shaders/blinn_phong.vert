#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_uv; 
layout (location = 2) in vec3 a_norm; 
  
out VS_OUT
{
    vec3 frag_pos;
    vec2 uv_coord;
    vec3 norm;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normal_matrix;

void main()
{
    vs_out.frag_pos = vec3(model * vec4(a_pos, 1.0));
    vs_out.uv_coord = a_uv;
    vs_out.norm = normal_matrix * a_norm;

    gl_Position = projection * view * model * vec4(a_pos, 1.0);
}