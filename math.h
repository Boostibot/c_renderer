#pragma once
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

#ifndef EPSILON
    #define EPSILON 2.0e-6f
#endif

#ifndef JMAPI
    #define JMAPI static inline
#endif

typedef struct Vec2
{
    float x;
    float y;
} Vec2;

typedef struct Vec3
{
    float x;
    float y;
    float z;
} Vec3;

typedef struct Vec4
{
    float x;
    float y;
    float z;
    float w;
} Vec4;

typedef struct Quat
{
    float x;
    float y;
    float z;
    float w;
} Quat;

typedef struct Mat2
{
    union {
        Vec2 col[2];

        struct {
            float _11, _21;
            float _12, _22;
        };
    };
} Mat2;

typedef struct Mat3
{
    union {
        Vec3 col[3];

        struct {
            float _11, _21, _31;
            float _12, _22, _32;
            float _13, _23, _33;
        };
    };
} Mat3;

typedef struct Mat4
{
    union {
        Vec4 col[4];

        struct {
            float _11, _21, _31, _41;
            float _12, _22, _32, _42;
            float _13, _23, _33, _43;
            float _14, _24, _34, _44;
        };
    };
} Mat4;

JMAPI float lerpf(float lo, float hi, float t) 
{
    return lo * (1.0f - t) + hi * t;
}

JMAPI float remapf(float value, float input_from, float input_to, float output_from, float output_to)
{
    float result = (value - input_from)/(input_to - input_from)*(output_to - output_from) + output_from;
    return result;
}

JMAPI bool equalsf_epsilon(float a, float b, float epsilon)
{
    return fabsf(a - b) <= epsilon;
}

JMAPI bool equalsf(float x, float y)
{
    float factor = 0.5f + fmaxf(fabsf(x), fabsf(y));
    return equalsf_epsilon(x, y, factor*EPSILON);
}

#define AS_FLOATS(vector)       (float*) (void*) &(vector)
#define AS_CONST_FLOATS(vector) (const float*) (void*) &(vector)

JMAPI Vec2 vec2_of(float scalar) { Vec2 out = {scalar, scalar}; return out; }
JMAPI Vec3 vec3_of(float scalar) { Vec3 out = {scalar, scalar, scalar}; return out; }
JMAPI Vec4 vec4_of(float scalar) { Vec4 out = {scalar, scalar, scalar, scalar}; return out; }

JMAPI Vec2 vec2_add(Vec2 a, Vec2 b) { Vec2 out = {a.x + b.x, a.y + b.y}; return out; }
JMAPI Vec3 vec3_add(Vec3 a, Vec3 b) { Vec3 out = {a.x + b.x, a.y + b.y, a.z + b.z}; return out; }
JMAPI Vec4 vec4_add(Vec4 a, Vec4 b) { Vec4 out = {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; return out; }

JMAPI Vec2 vec2_sub(Vec2 a, Vec2 b) { Vec2 out = {a.x - b.x, a.y - b.y}; return out; }
JMAPI Vec3 vec3_sub(Vec3 a, Vec3 b) { Vec3 out = {a.x - b.x, a.y - b.y, a.z - b.z}; return out; }
JMAPI Vec4 vec4_sub(Vec4 a, Vec4 b) { Vec4 out = {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; return out; }

JMAPI Vec2 vec2_scale(float scalar, Vec2 a) { Vec2 out = {scalar * a.x, scalar * a.y}; return out; }
JMAPI Vec3 vec3_scale(float scalar, Vec3 a) { Vec3 out = {scalar * a.x, scalar * a.y, scalar * a.z}; return out; }
JMAPI Vec4 vec4_scale(float scalar, Vec4 a) { Vec4 out = {scalar * a.x, scalar * a.y, scalar * a.z, scalar * a.w}; return out; }

JMAPI float vec2_dot(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }
JMAPI float vec3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
JMAPI float vec4_dot(Vec4 a, Vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

JMAPI float vec2_len(Vec2 a) { return sqrtf(vec2_dot(a, a)); }
JMAPI float vec3_len(Vec3 a) { return sqrtf(vec3_dot(a, a)); }
JMAPI float vec4_len(Vec4 a) { return sqrtf(vec4_dot(a, a)); }

JMAPI Vec2 vec2_norm(Vec2 a) { return vec2_scale(1.0f/vec2_len(a), a); }
JMAPI Vec3 vec3_norm(Vec3 a) { return vec3_scale(1.0f/vec3_len(a), a); }
JMAPI Vec4 vec4_norm(Vec4 a) { return vec4_scale(1.0f/vec4_len(a), a); }

JMAPI bool vec2_equals_exact(Vec2 a, Vec2 b) { return a.x==b.x && a.y==b.y; }
JMAPI bool vec3_equals_exact(Vec3 a, Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
JMAPI bool vec4_equals_exact(Vec4 a, Vec4 b) { return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w; }

JMAPI bool vec2_equals_epsilon(Vec2 a, Vec2 b, float epsilon) 
{
    return equalsf_epsilon(a.x, b.x, epsilon) 
        && equalsf_epsilon(a.y, b.y, epsilon);
}

JMAPI bool vec3_equals_epsilon(Vec3 a, Vec3 b, float epsilon) 
{
    return equalsf_epsilon(a.x, b.x, epsilon) 
        && equalsf_epsilon(a.y, b.y, epsilon) 
        && equalsf_epsilon(a.z, b.z, epsilon);
}

JMAPI bool vec4_equals_epsilon(Vec4 a, Vec4 b, float epsilon) 
{
    return equalsf_epsilon(a.x, b.x, epsilon) 
        && equalsf_epsilon(a.y, b.y, epsilon) 
        && equalsf_epsilon(a.z, b.z, epsilon) 
        && equalsf_epsilon(a.w, b.w, epsilon);
}

JMAPI bool vec2_equals(Vec2 a, Vec2 b) 
{
    return equalsf(a.x, b.x) 
        && equalsf(a.y, b.y);
}

JMAPI bool vec3_equals(Vec3 a, Vec3 b) 
{
    return equalsf(a.x, b.x) 
        && equalsf(a.y, b.y) 
        && equalsf(a.z, b.z);
}

JMAPI bool vec4_equals(Vec4 a, Vec4 b) 
{
    return equalsf(a.x, b.x) 
        && equalsf(a.y, b.y) 
        && equalsf(a.z, b.z) 
        && equalsf(a.w, b.w);
}

//For homogenous coords
JMAPI Vec4 vec4_homo(Vec3 a)         { Vec4 out = {a.x, a.y, a.z, 1}; return out; }
JMAPI Vec4 vec4_add3(Vec3 a, Vec3 b) { Vec4 out = {a.x + b.x, a.y + b.y, a.z + b.z, 1}; return out; } 
JMAPI Vec4 vec4_sub3(Vec3 a, Vec3 b) { Vec4 out = {a.x - b.x, a.y - b.y, a.z - b.z, 1}; return out; }

//Pairwise
#define m(a, b) a < b ? a : b
#define M(a, b) a < b ? a : b

JMAPI Vec2 vec2_pairwise_mul(Vec2 a, Vec2 b) { Vec2 out = {a.x * b.x, a.y * b.y};                               return out; }
JMAPI Vec3 vec3_pairwise_mul(Vec3 a, Vec3 b) { Vec3 out = {a.x * b.x, a.y * b.y, a.z * b.z};                    return out; }
JMAPI Vec4 vec4_pairwise_mul(Vec4 a, Vec4 b) { Vec4 out = {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};         return out; }

JMAPI Vec2 vec2_pairwise_min(Vec2 a, Vec2 b) { Vec2 out = {m(a.x, b.x), m(a.y, b.y)};                           return out; }
JMAPI Vec3 vec3_pairwise_min(Vec3 a, Vec3 b) { Vec3 out = {m(a.x, b.x), m(a.y, b.y), m(a.z, b.z)};              return out; }
JMAPI Vec4 vec4_pairwise_min(Vec4 a, Vec4 b) { Vec4 out = {m(a.x, b.x), m(a.y, b.y), m(a.z, b.z), m(a.w, b.w)}; return out; }

JMAPI Vec2 vec2_pairwise_max(Vec2 a, Vec2 b) { Vec2 out = {M(a.x, b.x), M(a.y, b.y)};                           return out; }
JMAPI Vec3 vec3_pairwise_max(Vec3 a, Vec3 b) { Vec3 out = {M(a.x, b.x), M(a.y, b.y), M(a.z, b.z)};              return out; }
JMAPI Vec4 vec4_pairwise_max(Vec4 a, Vec4 b) { Vec4 out = {M(a.x, b.x), M(a.y, b.y), M(a.z, b.z), M(a.w, b.w)}; return out; }

JMAPI Vec2 vec2_pairwise_clamp(Vec2 clamped, Vec2 low, Vec2 high) 
{ 
    Vec2 capped = vec2_pairwise_min(clamped, high);
    return vec2_pairwise_max(low, capped); 
}
JMAPI Vec3 vec3_pairwise_clamp(Vec3 clamped, Vec3 low, Vec3 high) 
{ 
    Vec3 capped = vec3_pairwise_min(clamped, high);
    return vec3_pairwise_max(low, capped); 
}

JMAPI Vec4 vec4_pairwise_clamp(Vec4 clamped, Vec4 low, Vec4 high) 
{ 
    Vec4 capped = vec4_pairwise_min(clamped, high);
    return vec4_pairwise_max(low, capped); 
}
#undef m
#undef M

JMAPI Vec2 vec2_lerp(Vec2 a, Vec2 b, float t)
{
    Vec2 result = vec2_add(vec2_scale(1.0f - t, a), vec2_scale(t, b)); 
    return result;
}

JMAPI Vec3 vec3_lerp(Vec3 a, Vec3 b, float t)
{
    Vec3 result = vec3_add(vec3_scale(1.0f - t, a), vec3_scale(t, b)); 
    return result;
}

JMAPI Vec4 vec4_lerp(Vec4 a, Vec4 b, float t)
{
    Vec4 result = vec4_add(vec4_scale(1.0f - t, a), vec4_scale(t, b)); 
    return result;
}

JMAPI Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    Vec3 result = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    return result;
}

JMAPI float vec2_angle_between(Vec2 a, Vec2 b)
{
    float len2a = vec2_dot(a, a);
    float len2b = vec2_dot(b, b);
    float den = sqrtf(len2a*len2b);
    float num = vec2_dot(a, b);
    float result = acosf(num/den);

    return result;
}

#if 1
JMAPI float vec3_angle_between(Vec3 a, Vec3 b)
{
    float len2a = vec3_dot(a, a);
    float len2b = vec3_dot(b, b);
    float den = sqrtf(len2a*len2b);
    float num = vec3_dot(a, b);
    float result = acosf(num/den);

    return result;
}
#else
JMAPI float vec3_angle_between(Vec3 a, Vec3 b)
{
    //source: raymath.h
    Vec3 crossed = vec3_cross(a, b);
    float cross_len = vec3_len(crossed);
    float dotted = vec3_dot(a, b);
    float result = atan2f(cross_len, dotted);

    return result;
}
#endif

//@NOTE: we declare all ops primarily on Mat4 and only provide Mat3 and Mat2 as
// shrunk down storagge types. We also provide wrappers that promote the given 
// matrix to Mat4 and hope the compiler will be smart enough to optimize all the 
// unneccessary ops result

JMAPI Vec2 mat4_mul_vec2(Mat4 mat, Vec2 vec)
{
    Vec2 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y;
    result.y = mat._21*vec.x + mat._22*vec.y;
    return result;
}

JMAPI Vec3 mat4_mul_vec3(Mat4 mat, Vec3 vec)
{
    Vec3 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y + mat._13*vec.z;
    result.y = mat._21*vec.x + mat._22*vec.y + mat._23*vec.z;
    result.z = mat._31*vec.x + mat._32*vec.y + mat._33*vec.z;
    return result;
}

JMAPI Vec4 mat4_mul_vec4(Mat4 mat, Vec4 vec)
{
    Vec4 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y + mat._13*vec.z + mat._14*vec.w;
    result.y = mat._21*vec.x + mat._22*vec.y + mat._23*vec.z + mat._24*vec.w;
    result.z = mat._31*vec.x + mat._32*vec.y + mat._33*vec.z + mat._34*vec.w;
    result.w = mat._41*vec.x + mat._42*vec.y + mat._43*vec.z + mat._44*vec.w;
    return result;
}

JMAPI Vec4 mat4_col(Mat4 matrix, ptrdiff_t column_i) 
{ 
    return matrix.col[column_i]; 
}

JMAPI Vec4 mat4_row(Mat4 matrix, int64_t row_i) 
{ 
    float* ptr = (&matrix._11) + row_i;
    Vec4 result = {ptr[0], ptr[4], ptr[8], ptr[12]};
    return result;
}

JMAPI Mat4 mat4_add(Mat4 a, Mat4 b)
{
    Mat4 result = {0};
    result.col[0] = vec4_add(a.col[0], b.col[0]);
    result.col[1] = vec4_add(a.col[1], b.col[1]);
    result.col[2] = vec4_add(a.col[2], b.col[2]);
    result.col[3] = vec4_add(a.col[3], b.col[3]);

    return result;
}

JMAPI Mat4 mat4_sub(Mat4 a, Mat4 b)
{
    Mat4 result = {0};
    result.col[0] = vec4_sub(a.col[0], b.col[0]);
    result.col[1] = vec4_sub(a.col[1], b.col[1]);
    result.col[2] = vec4_sub(a.col[2], b.col[2]);
    result.col[3] = vec4_sub(a.col[3], b.col[3]);

    return result;
}

JMAPI Mat4 mat4_scale(float scalar, Mat4 mat)
{
    Mat4 result = {0};
    result.col[0] = vec4_scale(scalar, mat.col[0]);
    result.col[1] = vec4_scale(scalar, mat.col[1]);
    result.col[2] = vec4_scale(scalar, mat.col[2]);
    result.col[3] = vec4_scale(scalar, mat.col[3]);

    return result;
}

JMAPI Mat4 mat4_mul(Mat4 a, Mat4 b)
{
    Mat4 result = {0};
    result.col[0] = mat4_mul_vec4(a, b.col[0]);
    result.col[1] = mat4_mul_vec4(a, b.col[1]);
    result.col[2] = mat4_mul_vec4(a, b.col[2]);
    result.col[3] = mat4_mul_vec4(a, b.col[3]);

    return result;
}

JMAPI bool mat4_equals_exact(Mat4 a, Mat4 b) 
{
    return memcmp(AS_FLOATS(a), AS_FLOATS(b), sizeof(Mat4)) == 0;
}

JMAPI bool mat4_equals_epsilon(Mat4 a, Mat4 b, float epsilon) 
{
    const float* a_ptr = AS_FLOATS(a);
    const float* b_ptr = AS_FLOATS(b);
    for(ptrdiff_t i = 0; i < 4*4; i++)
    {
        if(equalsf_epsilon(a_ptr[i], b_ptr[i], epsilon) == false)
            return false;
    }

    return true;
}

JMAPI bool mat4_equals(Mat4 a, Mat4 b) 
{
    const float* a_ptr = AS_FLOATS(a);
    const float* b_ptr = AS_FLOATS(b);
    for(ptrdiff_t i = 0; i < 4*4; i++)
    {
        if(equalsf(a_ptr[i], b_ptr[i]) == false)
            return false;
    }

    return true;
}

JMAPI Mat4 mat4_cols(Vec4 col1, Vec4 col2, Vec4 col3, Vec4 col4)
{
    Mat4 result = {col1, col2, col3, col4};
    return result;
}

JMAPI Mat4 mat4_rows(Vec4 row1, Vec4 row2, Vec4 row3, Vec4 row4)
{
    Mat4 result = {0};
    result._11 = row1.x;
    result._12 = row1.y;
    result._13 = row1.z;
    result._14 = row1.w;
    
    result._21 = row2.x;
    result._22 = row2.y;
    result._23 = row2.z;
    result._24 = row2.w;
    
    result._31 = row3.x;
    result._32 = row3.y;
    result._33 = row3.z;
    result._34 = row3.w;
    
    result._41 = row4.x;
    result._42 = row4.y;
    result._43 = row4.z;
    result._44 = row4.w;
    return result;
}

JMAPI Mat4 mat4_diagonal(Vec4 vec)
{
    Mat4 result = {0};
    result._11 = vec.x;
    result._22 = vec.y;
    result._33 = vec.z;
    result._44 = vec.w;
    return result;
}

JMAPI Mat4 mat4_identity()
{
    Vec4 diagonal = {1, 1, 1, 1};
    return mat4_diagonal(diagonal);
}

JMAPI Mat4 mat4_scaling(Vec3 vec)
{
    Vec4 diag = {vec.x, vec.y, vec.z, 1.0f};
    return mat4_diagonal(diag);
}

JMAPI Mat4 mat4_translation(Vec3 vec)
{
    Mat4 result = mat4_identity();
    result._41 = vec.x;
    result._42 = vec.y;
    result._43 = vec.z;
    return result;
}

JMAPI Mat4 mat4_rotation(Vec3 axis, float radians)
{
	float c = cosf(radians);
	float s = sinf(radians);

	Vec3 na = vec3_norm(axis);
    Vec3 t = vec3_scale(1.0f - c, na);

    Mat4 rotation = {0};
	rotation._11 = c + t.x * na.x;
	rotation._12 = t.x * na.y + s*na.z;
	rotation._13 = t.x * na.z - s*na.y;

	rotation._21 = t.y * na.x - s*na.z;
	rotation._22 = c + t.y * na.y;
	rotation._23 = t.y * na.z + s*na.x;

	rotation._31 = t.z * na.x + s*na.y;
	rotation._32 = t.z * na.y - s*na.x;
	rotation._33 = c + t.z * na.z;
    rotation._44 = 1.0f;
    return rotation;
}

//@NOTE: the order is reverse from glm!
//this means rotate(translate(mat, ...), ...) in glm first rotates and then translates
//here it first translates and then rotatets!
JMAPI Mat4 mat4_translate(Mat4 matrix, Vec3 offset)
{
    Vec4 homo_offset = vec4_homo(offset);
    Vec4 last = mat4_mul_vec4(matrix, homo_offset);
    Mat4 result = matrix;
    result.col[3] = last;
    return result;
}

JMAPI Mat4 mat4_rotate(Mat4 mat, Vec3 axis, float radians)
{
    Mat4 rotation = mat4_rotation(axis, radians);
    Mat4 result = mat4_mul(rotation, mat);
    return result;
}

JMAPI Mat4 mat4_scale_aniso(Mat4 mat, Vec3 scale_by)
{
    Mat4 result = {0};
    result.col[0] = vec4_scale(scale_by.x, mat.col[0]);
    result.col[1] = vec4_scale(scale_by.x, mat.col[1]);
    result.col[2] = vec4_scale(scale_by.x, mat.col[2]);
    result.col[3] = mat.col[3];
    return result;
}
