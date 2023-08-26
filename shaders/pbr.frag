#version 330 core

out vec4 frag_color;
  
in VS_OUT
{
    vec3 frag_pos;
    vec2 uv_coord;
    vec3 norm;
} fs_in;


#define PBR_MAX_LIGHTS 4

uniform vec3  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform float gamma;

uniform vec3  view_pos;
uniform vec3  lights_pos[PBR_MAX_LIGHTS];
uniform vec3  lights_color[PBR_MAX_LIGHTS];
uniform float lights_radius[PBR_MAX_LIGHTS];
uniform vec3  reflection_at_zero_incidence;

//uniform sampler2D texture_albedo;

const float PI = 3.14159265359;

vec3 gamma_correct(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

vec3 fresnel_schlick(float cos_theta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}  

float distribution_ggx(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometry_schlick_ggx(NdotV, roughness);
    float ggx1  = geometry_schlick_ggx(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float attentuate_square(float light_distance, float light_radius)
{
    return 1.0 / (light_distance * light_distance);
}

//from: http://www.cemyuksel.com/research/pointlightattenuation/
float attentuate_no_singularity(float light_distance, float light_radius)
{
    float d = light_distance;
    float r = light_radius;
    float d2 = d*d;
    float r2 = r*r;

    float result = 2 / (d2 + r2 + d*sqrt(d2 + r2));
    return result;
}

void main()
{
    //vec3  albedo = texture(texture_albedo, fs_in.uv_coord).rgb;
    //float metallic;
    //float roughness;
    //float ao;
    //float gamma;

    vec3 albedo_color = albedo;
    //albedo_color = gamma_correct(albedo, 1.0/gamma);
    vec3 N = normalize(fs_in.norm); 
    vec3 V = normalize(view_pos - fs_in.frag_pos);

    vec3 Lo = vec3(0.0);
    //int i = 0;
    for(int i = 0; i < PBR_MAX_LIGHTS; ++i) 
    {
        vec3 L = normalize(lights_pos[i] - fs_in.frag_pos);
        vec3 H = normalize(V + L);

        float distance    = length(lights_pos[i] - fs_in.frag_pos);
        float radius      = lights_radius[i];
        //float attenuation = attentuate_square(distance, radius);
        float attenuation = attentuate_no_singularity(distance, radius);
        vec3 radiance     = lights_color[i] * attenuation; 

        float cos_theta = max(dot(H, V), 0.0);
        vec3 F0 = mix(reflection_at_zero_incidence, albedo, metallic);
        vec3 F  = fresnel_schlick(cos_theta, F0);

        float NDF = distribution_ggx(N, H, roughness);       
        float G   = geometry_smith(N, V, L, roughness);  

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
        vec3 specular     = numerator / denominator;  

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS);
        kD *= 1.0 - metallic;	

        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo_color / PI + specular) * radiance * NdotL; 
    }
    
    frag_color = vec4(Lo, 1.0);
}