#pragma once

#include "lib/math.h"
#include "lib/array.h"
#include "lib/hash_index.h"
#include "lib/hash.h"

#ifndef VEC_ARRAY_DEFINED

    #define VEC_ARRAY_DEFINED
    typedef Array(Vec2) Vec2_Array;
    typedef Array(Vec3) Vec3_Array;
    typedef Array(Vec4) Vec4_Array;

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
    Quat rotate;
    Vec3 translate;
    Vec3 scale;
    float _pad[2];
} Transform;

typedef struct Uniform_Transform {
    Quat rotate;
    Vec3 translate;
    f32 scale;
} Uniform_Transform;

typedef Array(Vertex) Vertex_Array;
typedef Array(Shape_Vertex) Shape_Vertex_Array;
typedef Array(Triangle_Index) Triangle_Index_Array;
typedef Array(AABB) AABB_Array;
typedef Array(Transform) Transform_Array;

