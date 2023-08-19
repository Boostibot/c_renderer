#version 330 core
out vec4 frag_color;
  
in vec2 uv_coords;

uniform sampler2D screen;
uniform float gamma;
uniform float exposure;

void main()
{ 
    
    //frag_color = texture(screen, uv_coords);
    //return;

    //float exposure_ = 2.0;
    float exposure_ = exposure;

    vec3 color = texture(screen, uv_coords).xyz;
    vec3 mapped = vec3(1.0) - exp(-color * exposure_);
    mapped = pow(mapped, vec3(1.0 / gamma));

    frag_color = vec4(mapped, 1.0);
    //frag_color = vec4(1.0, .0, 1.0, 1.0);
}