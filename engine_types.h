#pragma once

#include "math.h"
#include "array.h"
#include "hash_index.h"
#include "hash.h"

#ifndef VEC_ARRAY_DEFINED

    #define VEC_ARRAY_DEFINED
    DEFINE_ARRAY_TYPE(Vec2, Vec2_Array);
    DEFINE_ARRAY_TYPE(Vec3, Vec3_Array);
    DEFINE_ARRAY_TYPE(Vec4, Vec4_Array);

#endif

typedef struct Small_Vertex
{
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
} Small_Vertex;

typedef struct Color_Vertex
{
    Vec3 pos;
    Vec4 color;
} Color_Vertex;

typedef struct Vertex
{
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
    Vec3 tan;
    Vec4 color;
} Vertex;

typedef struct Triangle_Index
{
    u32 vertex_i[3];
} Triangle_Index;

    
typedef struct AABB
{
    Vec3 min;
    Vec3 max;
} AABB;

typedef struct Transform
{
    Vec3 translate;
    Vec3 scale;
    Quat rotate;
} Transform;

DEFINE_ARRAY_TYPE(Vertex, Vertex_Array);
DEFINE_ARRAY_TYPE(Triangle_Index, Triangle_Index_Array);
DEFINE_ARRAY_TYPE(AABB, AABB_Array);
DEFINE_ARRAY_TYPE(Transform, Transform_Array);