#pragma once

#include "lib/math.h"
#include "lib/array.h"
#include "lib/hash_index.h"
#include "lib/hash.h"

#ifndef VEC_ARRAY_DEFINED

    #define VEC_ARRAY_DEFINED
    DEFINE_ARRAY_TYPE(Vec2, Vec2_Array);
    DEFINE_ARRAY_TYPE(Vec3, Vec3_Array);
    DEFINE_ARRAY_TYPE(Vec4, Vec4_Array);

#endif

typedef struct Shape_Vertex {
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
} Shape_Vertex;

typedef struct Vertex {
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
    Vec3 tan;
} Vertex;

typedef struct Triangle_Index {
    u32 vertex_i[3];
} Triangle_Index;
    
typedef struct AABB {
    Vec3 min;
    Vec3 max;
} AABB;

typedef struct Transform {
    Vec3 translate;
    Vec3 scale;
    Quat rotate;
} Transform;

typedef struct Uniform_Transform {
    Vec3 translate;
    f32 scale;
    Quat rotate;
} Uniform_Transform;

DEFINE_ARRAY_TYPE(Vertex, Vertex_Array);
DEFINE_ARRAY_TYPE(Shape_Vertex, Shape_Vertex_Array);
DEFINE_ARRAY_TYPE(Triangle_Index, Triangle_Index_Array);
DEFINE_ARRAY_TYPE(AABB, AABB_Array);
DEFINE_ARRAY_TYPE(Transform, Transform_Array);

