#version 330 core
layout (location = 0) in vec3 a_pos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 color;

out float dist_to_camera;
void main()
{
    //gl_Position = vec4(0);
    //vec4 cs_position = projection * view * model * vec4(a_pos, 1.0);
    vec4 cs_position = view * model * vec4(a_pos, 1.0);

    dist_to_camera = -cs_position.z;
    gl_Position = projection * cs_position;
    //gl_Position = cs_position;
} 
