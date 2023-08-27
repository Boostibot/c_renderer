#version 330 core
out vec4 frag_color;

in vec3 uv;

uniform samplerCube cubemap_diffuse;
uniform float gamma;

vec3 gamma_correct(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

void main()
{    
    vec4 texture_color = texture(cubemap_diffuse, uv);
    vec3 corrected_texture_color = gamma_correct(vec3(texture_color), 1.0/gamma);

    frag_color = vec4(corrected_texture_color, 1.0);
    //frag_color = vec4(uv, 1.0);
}