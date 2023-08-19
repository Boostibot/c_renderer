#ifndef LIB_MATH
#define LIB_MATH

#include <math.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif // !ASSERT

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

#ifndef TAU
    #define TAU (PI*2.0f)
#endif

#ifndef EPSILON
    #define EPSILON 2.0e-5f
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

typedef union Mat2
{
    Vec2 col[2];

    struct {
        float _11, _21;
        float _12, _22;
    };
} Mat2;

typedef union Mat3
{
    Vec3 col[3];

    struct {
        float _11, _21, _31;
        float _12, _22, _32;
        float _13, _23, _33;
    };
} Mat3;

typedef union Mat4
{
    Vec4 col[4];

    struct {
        float _11, _21, _31, _41;
        float _12, _22, _32, _42;
        float _13, _23, _33, _43;
        float _14, _24, _34, _44;
    };
} Mat4;


#ifndef __cplusplus
    #define VEC2(a, b) ((Vec2){a, b})
    #define VEC3(a, b, c) ((Vec3){a, b, c})
    #define VEC4(a, b, c, d) ((Vec4){a, b, c, d})
#else
    #define VEC2(a, b) (Vec2{a, b})
    #define VEC3(a, b, c) (Vec3{a, b, c})
    #define VEC4(a, b, c, d) (Vec4{a, b, c, d})
#endif

JMAPI float radiansf(float degrees)
{
    float radians = degrees / 180.0f * PI;
    return radians;
}

JMAPI float degreesf(float radians)
{
    float degrees = radians * 180.0f / PI;
    return degrees;
}

JMAPI float lerpf(float lo, float hi, float t) 
{
    return lo * (1.0f - t) + hi * t;
}

JMAPI float remapf(float value, float input_from, float input_to, float output_from, float output_to)
{
    float result = (value - input_from)/(input_to - input_from)*(output_to - output_from) + output_from;
    return result;
}

JMAPI bool is_nearf(float a, float b, float epsilon)
{
    return fabsf(a - b) <= epsilon;
}

JMAPI float epsilon_factorf(float x, float y)
{
    float factor = 0.5f + fmaxf(fabsf(x), fabsf(y));
    return factor;
}

JMAPI bool is_near_scaledf(float x, float y, float epsilon)
{
    float factor = epsilon_factorf(x, y);
    return is_nearf(x, y, factor*epsilon);
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

JMAPI bool vec2_is_equal(Vec2 a, Vec2 b) { return memcmp(&a, &b, sizeof a) == 0; }
JMAPI bool vec3_is_equal(Vec3 a, Vec3 b) { return memcmp(&a, &b, sizeof a) == 0; }
JMAPI bool vec4_is_equal(Vec4 a, Vec4 b) { return memcmp(&a, &b, sizeof a) == 0; }

JMAPI Vec2 vec2_norm(Vec2 a) { ASSERT(vec2_len(a) > 0.0f); return vec2_scale(1.0f/vec2_len(a), a); }
JMAPI Vec3 vec3_norm(Vec3 a) { ASSERT(vec3_len(a) > 0.0f); return vec3_scale(1.0f/vec3_len(a), a); }
JMAPI Vec4 vec4_norm(Vec4 a) { ASSERT(vec4_len(a) > 0.0f); return vec4_scale(1.0f/vec4_len(a), a); }

JMAPI bool vec2_is_near(Vec2 a, Vec2 b, float epsilon) 
{
    return is_nearf(a.x, b.x, epsilon) 
        && is_nearf(a.y, b.y, epsilon);
}

JMAPI bool vec3_is_near(Vec3 a, Vec3 b, float epsilon) 
{
    return is_nearf(a.x, b.x, epsilon) 
        && is_nearf(a.y, b.y, epsilon) 
        && is_nearf(a.z, b.z, epsilon);
}

JMAPI bool vec4_is_near(Vec4 a, Vec4 b, float epsilon) 
{
    return is_nearf(a.x, b.x, epsilon) 
        && is_nearf(a.y, b.y, epsilon) 
        && is_nearf(a.z, b.z, epsilon) 
        && is_nearf(a.w, b.w, epsilon);
}

JMAPI bool vec2_is_near_scaled(Vec2 a, Vec2 b, float epsilon) 
{
    return is_near_scaledf(a.x, b.x, epsilon) 
        && is_near_scaledf(a.y, b.y, epsilon);
}

JMAPI bool vec3_is_near_scaled(Vec3 a, Vec3 b, float epsilon) 
{
    return is_near_scaledf(a.x, b.x, epsilon) 
        && is_near_scaledf(a.y, b.y, epsilon) 
        && is_near_scaledf(a.z, b.z, epsilon);
}

JMAPI bool vec4_is_near_scaled(Vec4 a, Vec4 b, float epsilon) 
{
    return is_near_scaledf(a.x, b.x, epsilon) 
        && is_near_scaledf(a.y, b.y, epsilon) 
        && is_near_scaledf(a.z, b.z, epsilon) 
        && is_near_scaledf(a.w, b.w, epsilon);
}

//For homogenous coords
JMAPI Vec4 vec4_from_vec3(Vec3 a)       { Vec4 out = {a.x, a.y, a.z, 0}; return out; }
JMAPI Vec4 vec4_from_homo_vec3(Vec3 a)  { Vec4 out = {a.x, a.y, a.z, 1}; return out; }
JMAPI Vec4 vec4_add3(Vec3 a, Vec3 b)    { Vec4 out = {a.x + b.x, a.y + b.y, a.z + b.z, 1}; return out; } 
JMAPI Vec4 vec4_sub3(Vec3 a, Vec3 b)    { Vec4 out = {a.x - b.x, a.y - b.y, a.z - b.z, 1}; return out; }

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

#undef m
#undef M

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

#if 0
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
    //@NOTE: this implementation is a lot more accurate than the one above
    //source: raymath.h
    Vec3 crossed = vec3_cross(a, b);
    float cross_len = vec3_len(crossed);
    float dotted = vec3_dot(a, b);
    float result = atan2f(cross_len, dotted);

    return result;
}
#endif

JMAPI Vec4 mat4_mul_vec4(Mat4 mat, Vec4 vec)
{
    Vec4 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y + mat._13*vec.z + mat._14*vec.w;
    result.y = mat._21*vec.x + mat._22*vec.y + mat._23*vec.z + mat._24*vec.w;
    result.z = mat._31*vec.x + mat._32*vec.y + mat._33*vec.z + mat._34*vec.w;
    result.w = mat._41*vec.x + mat._42*vec.y + mat._43*vec.z + mat._44*vec.w;
    return result;
}

JMAPI Vec4 mat4_col(Mat4 matrix, int64_t column_i) 
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

JMAPI bool mat4_is_equal(Mat4 a, Mat4 b) 
{
    return memcmp(&a, &b, sizeof a) == 0;
}

JMAPI bool mat4_is_near(Mat4 a, Mat4 b, float epsilon) 
{
    const float* a_ptr = AS_FLOATS(a);
    const float* b_ptr = AS_FLOATS(b);
    for(int i = 0; i < 4*4; i++)
    {
        if(is_nearf(a_ptr[i], b_ptr[i], epsilon) == false)
            return false;
    }

    return true;
}

JMAPI bool mat4_is_near_scaled(Mat4 a, Mat4 b, float epsilon) 
{
    const float* a_ptr = AS_FLOATS(a);
    const float* b_ptr = AS_FLOATS(b);
    for(int i = 0; i < 4*4; i++)
    {
        if(is_near_scaledf(a_ptr[i], b_ptr[i], epsilon) == false)
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

//@NOTE: the application order is reverse from glm!
//this means rotate(translate(mat, ...), ...)
// glm:  first rotates and then translates
// here: first translates and then rotatets!
JMAPI Mat4 mat4_translate(Mat4 matrix, Vec3 offset)
{
    Vec4 homo_offset = vec4_from_homo_vec3(offset);
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

//Makes a perspective projection matrix so that the output is in ranage [-1, 1] in all dimensions (OpenGL standard)
JMAPI Mat4 mat4_perspective_projection(float fov, float aspect_ratio, float near, float far) 
{ 
    ASSERT(fov != 0);
    ASSERT(near != far);
    ASSERT(aspect_ratio != 0);

    //https://ogldev.org/www/tutorial12/tutorial12.html
    const float tan_half_fov = tanf(fov / 2.0f);

    Mat4 result = {0};
    result._11 = 1.0f / (tan_half_fov * aspect_ratio);
    result._22 = 1.0f / tan_half_fov;
    result._33 = (-near - far) / (near - far);
    result._34 = 2.0f * far * near / (near - far);
    result._43 = 1.0f;
    return result;
} 

JMAPI Mat4 mat4_ortographic_projection(float bot, float top, float left, float righ, float near, float far) 
{
    Mat4 result = {0};
    ASSERT(bot != top);
    ASSERT(left != righ);
    ASSERT(near != far);

    result._11 = 2.0f / (righ - left); 
 
    result._22 = 2.0f / (top - bot); 
 
    result._33 = -2.0f / (far - near); 
 
    result._41 = -(righ + left) / (righ - left); 
    result._42 = -(top + bot) / (top - bot); 
    result._43 = -(far + near) / (far - near); 
    result._44 = 1; 
    return result;
}

JMAPI Mat4 mat4_just_look_at(Vec3 front_dir, Vec3 up_dir)
{
    //front_dir = vec3_scale(-1, front_dir);

    const Vec3 n = vec3_norm(front_dir);
    const Vec3 u = vec3_norm(vec3_cross(front_dir, up_dir));
    const Vec3 v = vec3_cross(u, n);

    Mat4 m = {0};
    m._11 = u.x;  m._12 = u.y;  m._13 = u.z;  m._14 = 0.0f; 
    m._21 = v.x;  m._22 = v.y;  m._23 = v.z;  m._24 = 0.0f; 
    m._31 = n.x;  m._32 = n.y;  m._33 = n.z;  m._34 = 0.0f; 
    m._41 = 0.0f; m._42 = 0.0f; m._43 = 0.0f; m._44 = 1.0f; 

    return m;
}

JMAPI Mat4 mat4_look_at(Vec3 camera_pos, Vec3 camera_target, Vec3 camera_up_dir)
{
    const Vec3 front_dir = vec3_sub(camera_target, camera_pos);
    Mat4 m = mat4_just_look_at(front_dir, camera_up_dir);
    Mat4 look_at = mat4_translate(m, vec3_scale(-1.0f, camera_pos));

    return look_at;
}

#endif
