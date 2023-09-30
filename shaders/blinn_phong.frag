#version 330 core

out vec4 frag_color;
  
in VS_OUT
{
    vec3 frag_pos;
    vec2 uv;
    vec3 norm;
} fs_in;

uniform vec3  view_pos;
uniform vec3  light_pos;
uniform vec3  light_color;
uniform float light_ambient_strength;
uniform float light_specular_sharpness;
uniform float light_specular_strength;
uniform float light_specular_effect; //between 0 and 1. if 0 uses only light color for specular, if 1 uses only diffuse for specular
uniform float light_linear_attentuation;
uniform float light_quadratic_attentuation;
uniform float gamma;

uniform sampler2D map_diffuse;

vec3 gamma_correct(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

void main()
{
    vec3 texture_color = texture(map_diffuse, fs_in.uv).rgb;
    vec3 corrected_texture_color = gamma_correct(texture_color, 1.0/gamma);

    vec3 diffuse_color = corrected_texture_color * light_color;
    vec3 specular_color = mix(light_color, diffuse_color, light_specular_effect);

    vec3 light_dir = normalize(light_pos - fs_in.frag_pos);
    vec3 normal = normalize(fs_in.norm);
    vec3 view_dir = normalize(view_pos - fs_in.frag_pos);
    vec3 reflect_dir = reflect(-light_dir, normal);
    vec3 halfway_dir = normalize(light_dir + view_dir);  

    float ambient_mult = light_ambient_strength;
    float diffuse_mult = max(dot(light_dir, normal), 0.0);
    float specular_mult = pow(max(dot(normal, halfway_dir), 0.0), light_specular_sharpness);
    
    //if is not lit at all cannot have a specular
    if(diffuse_mult <= 0)
        specular_mult = 0;

    //attentuation
    float light_dist = length(light_pos - view_pos);
    float atten_lin = light_linear_attentuation;
    float atten_qua = light_quadratic_attentuation;
    float attentuation = 1 / (1 + light_dist*light_dist*atten_qua + light_dist*atten_lin);
    attentuation = 1;

    vec3 ambient = diffuse_color * ambient_mult * attentuation;
    vec3 diffuse = diffuse_color * diffuse_mult * attentuation;
    vec3 specular = specular_color * specular_mult * attentuation * light_specular_strength;

    vec3 result = ambient + diffuse + specular;
    //result = vec3(diffuse_mult, diffuse_mult, diffuse_mult);
    //result = view_dir;
    //result = light_dir;
    //result = diffuse_color;
    //result = vec3(fs_in.uv, 0);
    //result = normal;
    //result = texture_color;
    frag_color = vec4(result, 1.0);
    return;
}