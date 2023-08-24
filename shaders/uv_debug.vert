#version 330 core
// the color variable has attribute position 1
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_uv; 
layout (location = 2) in vec3 a_norm; 
layout (location = 3) in vec3 a_tan; 
layout (location = 4) in vec3 a_bitan; 
  
out VS_OUT
{
    vec3 norm;
    vec3 tan;
    vec3 bitan;

} vs_out;

uniform vec3 view_pos;
uniform vec3 light_pos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normal_matrix;

void main()
{
    vs_out.norm = normal_matrix * a_norm;
    vs_out.tan = normal_matrix * a_tan;
    vs_out.bitan = normal_matrix * a_bitan;

    gl_Position = model * vec4(a_pos, 1.0);
}