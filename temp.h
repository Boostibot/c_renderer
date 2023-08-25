#pragma once

#include "math.h"
#include "defines.h"

typedef struct Spherical_Vec
{
    f32 r;
    f32 phi;
    f32 theta;
} Spherical_Vec;

Spherical_Vec vec3_to_spherical(Vec3 vec)
{
    Spherical_Vec result = {0};
    result.r = vec3_len(vec);
    result.phi = atan2f(vec.x,vec.z);
    result.theta = atan2f(vec.y, hypotf(vec.x,vec.z));
    return result;
}

Vec3 vec3_from_spherical(Spherical_Vec spherical)
{
    Vec3 result = {0};
    result.z = cosf(spherical.phi) * cosf(spherical.theta) * spherical.r;
    result.y = sinf(spherical.theta) * spherical.r;
    result.x = sinf(spherical.phi) * cosf(spherical.theta) * spherical.r;

    return result;
}

f32 vec3_p_len(Vec3 vec, f32 p)
{
    f32 x = fabsf(vec.x);
    f32 y = fabsf(vec.y);
    f32 z = fabsf(vec.z);
    f32 sum = powf(x, p) + powf(y, p) + powf(z, p);
    f32 result = powf(sum, 1.0f/p);
    
    return result;
}

Vec3 vec3_p_norm(Vec3 vec, f32 p)
{
    f32 p_len = vec3_p_len(vec, p);
    ASSERT(p_len != 0);
    Vec3 result = vec3_scale(vec, 1.0f/p_len);
    return result;
}
