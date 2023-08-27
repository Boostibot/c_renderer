#version 330 core
out vec4 frag_color;
  
in vec2 uv;

uniform sampler2D screen;
uniform float gamma;
uniform float exposure;

void main()
{ 
    float exposure_ = exposure;

    vec3 color = texture(screen, uv).xyz;
    vec3 mapped = vec3(1.0) - exp(-color * exposure_);
    mapped = pow(mapped, vec3(1.0 / gamma));
    //mapped *= 0.5;
    frag_color = vec4(mapped, 1.0);
}