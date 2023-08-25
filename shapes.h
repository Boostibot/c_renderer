#pragma once

#include "math.h"
#include "array.h"
#include "hash_index.h"
#include "hash.h"

typedef struct Vertex
{
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
} Vertex;

typedef struct Full_Vertex
{
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
    Vec3 tan;
    Vec3 bitan;
    Vec3 color;
} Full_Vertex;

typedef struct Triangle_Indeces
{
    u32 vertex_i[3];
} Triangle_Indeces;

DEFINE_ARRAY_TYPE(Vertex, Vertex_Array);
DEFINE_ARRAY_TYPE(Triangle_Indeces, Triangle_Indeces_Array);

typedef struct Shape
{
    Vertex_Array vertices;
    Triangle_Indeces_Array indeces;
} Shape;

void shape_deinit(Shape* shape)
{
    array_deinit(&shape->vertices);
    array_deinit(&shape->indeces);
}

Shape shape_duplicate(Shape from)
{
    Shape out = {0};
    array_copy(&out.vertices, from.vertices);
    array_copy(&out.indeces, from.indeces);
    return out;
}

void shape_tranform(Shape* shape, Mat4 transform)
{
    Mat4 normal_matrix = mat4_inverse_nonuniform_scale(transform);

    for(isize i = 0; i < shape->vertices.size; i++)
    {
        Vertex* vertex = &shape->vertices.data[i];
        vertex->pos = mat4_apply(transform, vertex->pos);
        vertex->norm = mat4_mul_vec3(normal_matrix, vertex->norm);
    }
}


#define E 0.5f
const Vertex XZ_QUAD_VERTICES[] = {
     E, 0,  E, {1, 1}, {0, 1, 0},
    -E, 0,  E, {1, 0}, {0, 1, 0}, 
    -E, 0, -E, {0, 0}, {0, 1, 0},
     E, 0, -E, {0, 1}, {0, 1, 0},
};

const Triangle_Indeces XZ_QUAD_INDECES[] = { 
    1, 0, 2, 0, 3, 2
};

const Vertex CUBE_VERTICES[] = {
    // Back face
    -E, -E, -E,  0.0f, 0.0f, {0, 0, -1}, // Bottom-left
     E,  E, -E,  1.0f, 1.0f, {0, 0, -1}, // top-right
     E, -E, -E,  1.0f, 0.0f, {0, 0, -1}, // bottom-right         
     E,  E, -E,  1.0f, 1.0f, {0, 0, -1}, // top-right
    -E, -E, -E,  0.0f, 0.0f, {0, 0, -1}, // bottom-left
    -E,  E, -E,  0.0f, 1.0f, {0, 0, -1}, // top-left
    // Front face
    -E, -E,  E,  0.0f, 0.0f, {0, 0, 1}, // bottom-left
     E, -E,  E,  1.0f, 0.0f, {0, 0, 1}, // bottom-right
     E,  E,  E,  1.0f, 1.0f, {0, 0, 1}, // top-right
     E,  E,  E,  1.0f, 1.0f, {0, 0, 1}, // top-right
    -E,  E,  E,  0.0f, 1.0f, {0, 0, 1}, // top-left
    -E, -E,  E,  0.0f, 0.0f, {0, 0, 1}, // bottom-left
    // Left face
    -E,  E,  E,  1.0f, 0.0f, {-1, 0, 0}, // top-right
    -E,  E, -E,  1.0f, 1.0f, {-1, 0, 0}, // top-left
    -E, -E, -E,  0.0f, 1.0f, {-1, 0, 0}, // bottom-left
    -E, -E, -E,  0.0f, 1.0f, {-1, 0, 0}, // bottom-left
    -E, -E,  E,  0.0f, 0.0f, {-1, 0, 0}, // bottom-right
    -E,  E,  E,  1.0f, 0.0f, {-1, 0, 0}, // top-right
    // Right face
     E,  E,  E,  1.0f, 0.0f, {1, 0, 0}, // top-left
     E, -E, -E,  0.0f, 1.0f, {1, 0, 0}, // bottom-right
     E,  E, -E,  1.0f, 1.0f, {1, 0, 0}, // top-right         
     E, -E, -E,  0.0f, 1.0f, {1, 0, 0}, // bottom-right
     E,  E,  E,  1.0f, 0.0f, {1, 0, 0}, // top-left
     E, -E,  E,  0.0f, 0.0f, {1, 0, 0}, // bottom-left     
    // Bottom face
    -E, -E, -E,  0.0f, 1.0f, {0, -1, 0}, // top-right
     E, -E, -E,  1.0f, 1.0f, {0, -1, 0}, // top-left
     E, -E,  E,  1.0f, 0.0f, {0, -1, 0}, // bottom-left
     E, -E,  E,  1.0f, 0.0f, {0, -1, 0}, // bottom-left
    -E, -E,  E,  0.0f, 0.0f, {0, -1, 0}, // bottom-right
    -E, -E, -E,  0.0f, 1.0f, {0, -1, 0}, // top-right
    // Top face
    -E,  E, -E,  0.0f, 1.0f, {0, 1, 0}, // top-left
     E,  E,  E,  1.0f, 0.0f, {0, 1, 0}, // bottom-right
     E,  E, -E,  1.0f, 1.0f, {0, 1, 0}, // top-right     
     E,  E,  E,  1.0f, 0.0f, {0, 1, 0}, // bottom-right
    -E,  E, -E,  0.0f, 1.0f, {0, 1, 0}, // top-left
    -E,  E,  E,  0.0f, 0.0f, {0, 1, 0}  // bottom-left        
};

#undef E

const Triangle_Indeces CUBE_INDECES[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35
};

u64 vertex_hash64(Vertex vertex, u64 seed)
{
    u64 out = hash64_murmur(&vertex, sizeof vertex, seed);
    return out;
}

Shape shapes_make_unit_quad()
{
    Shape out = {0};
    isize vertex_count = STATIC_ARRAY_SIZE(XZ_QUAD_VERTICES);
    isize index_count = STATIC_ARRAY_SIZE(XZ_QUAD_INDECES);
    array_append(&out.vertices, XZ_QUAD_VERTICES, vertex_count);
    array_append(&out.indeces, XZ_QUAD_INDECES, index_count);

    return out;
}

//makes a cube in spanning [-0.5, 0.5]^3 centered at origin
Shape shapes_make_unit_cube()
{
    
    Shape out = {0};
    isize vertex_count = STATIC_ARRAY_SIZE(CUBE_VERTICES);
    isize index_count = STATIC_ARRAY_SIZE(CUBE_INDECES);
    array_append(&out.vertices, CUBE_VERTICES, vertex_count);
    array_append(&out.indeces, CUBE_INDECES, index_count);

    return out;
}

Shape shapes_make_cube(f32 side_size, Vec3 up_dir, Vec3 front_dir, Vec3 offset)
{
    Shape cube = shapes_make_unit_cube();
    Mat4 transform = mat4_local_matrix(front_dir, up_dir, offset);
    transform = mat4_scale_affine(transform, vec3_of(side_size));
    shape_tranform(&cube, transform);
    return cube;
}

Shape shapes_make_quad(f32 side_size, Vec3 up_dir, Vec3 front_dir, Vec3 offset)
{
    Shape quad = shapes_make_unit_quad();
    Mat4 transform = mat4_local_matrix(front_dir, up_dir, offset);
    transform = mat4_scale_affine(transform, vec3_of(side_size));
    shape_tranform(&quad, transform);
    return quad;
}

typedef enum Winding_Order
{
    WINDING_ORDER_CLOCKWISE,
    WINDING_ORDER_COUNTER_CLOCKWISE,
} Winding_Order;

Winding_Order get_winding_order(Vertex v1, Vertex v2, Vertex v3)
{
    Vec3 avg_norm = vec3_add(vec3_add(v1.norm, v2.norm), v3.norm);
    avg_norm = vec3_norm(avg_norm);

    Vec3 edge1 = vec3_sub(v2.pos, v1.pos);
    Vec3 edge2 = vec3_sub(v3.pos, v1.pos);

    Vec3 triangle_right_norm = vec3_cross(edge1, edge2);
    f32 similarity = vec3_dot(avg_norm, triangle_right_norm);

    if(similarity < 0)
        return WINDING_ORDER_COUNTER_CLOCKWISE;
    else
        return WINDING_ORDER_CLOCKWISE;
}

Winding_Order get_winding_order_index(Vertex* vertices, Triangle_Indeces trinagle)
{
    Vertex v1 = vertices[trinagle.vertex_i[0]];
    Vertex v2 = vertices[trinagle.vertex_i[1]];
    Vertex v3 = vertices[trinagle.vertex_i[2]];

    return get_winding_order(v1, v2, v3);
}

void shapes_add_cube_sphere_side(Shape* into, isize iters, f32 radius, Vec3 side_normal, Vec3 side_front, Vec3 offset)
{
    if(iters <= 0)
        return;

    isize vertices_before = into->vertices.size;
    isize indeces_before = into->indeces.size;

    const Vec3 origin = vec3(0, 0, 0);
    const Vec3 corners[4] = {
        vec3(-1, 1, -1),
        vec3(1, 1, -1),
        vec3(-1, 1, 1),
        vec3(1, 1, 1),
    };

    const f32 y_angle = vec3_angle_between(vec3_sub(corners[0], origin), vec3_sub(corners[1], origin));
    const f32 x_angle = vec3_angle_between(vec3_sub(corners[2], origin), vec3_sub(corners[3], origin));
    
    array_reserve(&into->indeces, indeces_before + (iters) * (iters) * 2);
    array_reserve(&into->vertices, vertices_before + (iters + 1) * (iters + 1));
    
    const Mat4 local_matrix = mat4_local_matrix(side_front, side_normal, offset);
    for(isize y = 0; y <= iters; y++)
    {
        f32 y_t = (f32) y / (f32) iters;
        Vec3 vertical_blend = vec3_slerp(corners[0], corners[1], y_angle, y_t);
        Vec3 horizontal_blend = vec3_slerp(corners[2], corners[3], x_angle, y_t);

        for(isize x = 0; x <= iters; x++)
        {
            f32 x_t = (f32) x / (f32) iters;
            Vec3 final_blend = vec3_norm(vec3_slerp_around(vertical_blend, horizontal_blend, origin, x_t));
            Vec3 position = mat4_apply(local_matrix, final_blend);
            Vec3 norm = mat4_mul_vec3(local_matrix, final_blend);

            Vertex vertex = {0};
            vertex.uv = vec2(y_t, x_t);
            vertex.norm = norm;
            vertex.pos = vec3_scale(position, radius);
            
            array_push(&into->vertices, vertex);
        }
    }
    
    ASSERT(iters > 0);
    u32 uiters = (u32) iters;
    u32 ubefore = (u32) vertices_before;
    for (u32 y = 0; y < uiters; y++)
    {
        for (u32 x = 0; x < uiters; x++)
        {
            Triangle_Indeces tri1 = {0}; 
            tri1.vertex_i[0] = (y + 1) * (uiters + 1) + x + ubefore;
            tri1.vertex_i[1] = y       * (uiters + 1) + x + ubefore;
            tri1.vertex_i[2] = y       * (uiters + 1) + x + 1 + ubefore;
            
            Triangle_Indeces tri2 = {0};
            tri2.vertex_i[0] = (y + 1) * (uiters + 1) + x + ubefore;
            tri2.vertex_i[1] = y       * (uiters + 1) + x + 1 + ubefore;
            tri2.vertex_i[2] = (y + 1) * (uiters + 1) + x + 1 + ubefore;

            array_push(&into->indeces, tri1);
            array_push(&into->indeces, tri2);
            
            ASSERT(get_winding_order_index(into->vertices.data, tri1) == WINDING_ORDER_CLOCKWISE);
            ASSERT(get_winding_order_index(into->vertices.data, tri2) == WINDING_ORDER_CLOCKWISE);
        }
        
    }
}

Shape shapes_make_cube_sphere_side(isize iters, f32 radius, Vec3 side_normal, Vec3 side_front, Vec3 offset)
{
    Shape out = {0};
    shapes_add_cube_sphere_side(&out, iters, radius, side_normal, side_front, offset);
    return out;
}

Shape shapes_make_cube_sphere(isize iters, f32 radius)
{
    Shape out = {0};
    array_reserve(&out.indeces, (iters) * (iters) * 2 * 6);
    array_reserve(&out.vertices, (iters + 1) * (iters + 1) * 6);

    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 1, 0), vec3(1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(1, 0, 0), vec3(0, 0, 1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, 1), vec3(1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, -1, 0), vec3(-1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(-1, 0, 0), vec3(0, 0, -1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, -1), vec3(-1, 0, 0), vec3_of(0));

    return out;
}

Shape shapes_make_voleyball_sphere(isize iters, f32 radius)
{
    Shape out = {0};
    array_reserve(&out.indeces, (iters) * (iters) * 2 * 6);
    array_reserve(&out.vertices, (iters + 1) * (iters + 1) * 6);

    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 1, 0), vec3(1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(1, 0, 0), vec3(0, 0, 1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, 1), vec3(0, 1, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, -1, 0), vec3(-1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(-1, 0, 0), vec3(0, 0, -1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, -1), vec3(0, -1, 0), vec3_of(0));

    return out;
}

Shape shapes_make_uv_sphere(isize iters, f32 radius)
{
    ASSERT(radius > 0);
    ASSERT(iters >= 0);

    //add the initial indeces
    Vertex_Array vertices = {0};
    Triangle_Indeces_Array indeces = {0};

    u32 y_segments = (u32) iters;
    u32 x_segments = (u32) iters;
    for (u32 y = 0; y <= y_segments; y++)
    {
        for (u32 x = 0; x <= x_segments; x++)
        {
            f32 x_segment = (f32)x / (f32)x_segments;
            f32 y_segment = (f32)y / (f32)y_segments;

            f32 phi = x_segment * TAU;
            f32 theta = y_segment * TAU/2;

            f32 x_pos = cosf(phi) * sinf(theta);
            f32 y_pos = cosf(theta);
            f32 z_pos = sinf(phi) * sinf(theta);

            Vertex vertex = {0};
            vertex.pos = vec3(x_pos, y_pos, z_pos);
            vertex.uv = vec2(x_segment, y_segment);
            vertex.norm = vec3(x_pos, y_pos, z_pos);
            array_push(&vertices, vertex);
        }
    }

    ASSERT(iters > 0);
    Shape out_shape = {0};
    for (u32 y = 0; y < y_segments; y++)
    {
        for (u32 x = 0; x < x_segments; x++)
        {
            Triangle_Indeces tri1 = {0}; 
            tri1.vertex_i[0] = (y + 1) * (x_segments + 1) + x;
            tri1.vertex_i[1] = y       * (x_segments + 1) + x;
            tri1.vertex_i[2] = y       * (x_segments + 1) + x + 1;
            
            Triangle_Indeces tri2 = {0};
            tri2.vertex_i[0] = (y + 1) * (x_segments + 1) + x;
            tri2.vertex_i[1] = y       * (x_segments + 1) + x + 1;
            tri2.vertex_i[2] = (y + 1) * (x_segments + 1) + x + 1;
            array_push(&indeces, tri1);
            array_push(&indeces, tri2);

            ASSERT(get_winding_order_index(vertices.data, tri1) == WINDING_ORDER_CLOCKWISE);
            //ASSERT(get_winding_order_index(vertices.data, tri2) == WINDING_ORDER_CLOCKWISE);
        }
        
    }

    out_shape.indeces = indeces;
    out_shape.vertices = vertices;
    return out_shape;
}

//Shape shapes_make_sphere(isize iters, f32 radius);

void shapes_make_icosahedron()
{
    #define X 0.525731112119133606f
    #define Z 0.850650808352039932f
    #define N 0.f

    static const Vec3 vertices[] = {
        {-X,N,Z}, {X,N,Z}, {-X,N,-Z}, {X,N,-Z},
        {N,Z,X}, {N,Z,-X}, {N,-Z,X}, {N,-Z,-X},
        {Z,X,N}, {-Z,X, N}, {Z,-X,N}, {-Z,-X, N}
    };

    static const Triangle_Indeces triangles[] = {
        {0,4,1},{0,9,4},{9,5,4},{4,5,8},{4,8,1},
        {8,10,1},{8,3,10},{5,3,8},{5,2,3},{2,7,3},
        {7,10,3},{7,6,10},{7,11,6},{11,0,6},{0,1,6},
        {6,1,10},{9,0,11},{9,11,2},{9,2,5},{7,2,11}
    };

    #undef X
    #undef Z
    #undef N
}