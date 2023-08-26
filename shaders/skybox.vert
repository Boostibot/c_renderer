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
    //flip z because all cubemaps use left handed systems in opengl
    uv_coord = vec3(world_pos.xy, -world_pos.z);

    vec4 pos = projection * view * world_pos;
    gl_Position = pos.xyww;
}