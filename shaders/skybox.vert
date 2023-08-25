#version 330 core

layout (location = 0) in vec3 a_pos;
  
out vec3 uv_coord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float gamma;

void main()
{
    vec4 world_pos = model * vec4(a_pos, 1.0);
    uv_coord.xyz = world_pos.xyz;

    vec4 pos = projection * view * world_pos;
    gl_Position = pos.xyww;
}