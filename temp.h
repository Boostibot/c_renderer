#pragma once

#include "math.h"
#include "defines.h"

//useful: https://catlikecoding.com/unity/tutorials/

static f32 vec3_p_len(Vec3 vec, f32 p)
{
    f32 x = fabsf(vec.x);
    f32 y = fabsf(vec.y);
    f32 z = fabsf(vec.z);
    f32 sum = powf(x, p) + powf(y, p) + powf(z, p);
    f32 result = powf(sum, 1.0f/p);
    
    return result;
}

static Vec3 vec3_p_norm(Vec3 vec, f32 p)
{
    f32 p_len = vec3_p_len(vec, p);
    ASSERT(p_len != 0);
    Vec3 result = vec3_scale(vec, 1.0f/p_len);
    return result;
}
