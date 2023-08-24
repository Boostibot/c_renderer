#version 330 core
out vec4 frag_color;
 
in GS_OUT
{
    vec3 color;
} fs_in;

void main()
{
    frag_color = vec4(fs_in.color, 1.0);
    //frag_color = vec4(1.0, 0.0, 0.0, 1.0);
    return;
}