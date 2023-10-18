#pragma once

#include "math.h"
#include "array.h"
#include "hash_index.h"
#include "hash.h"
#include "engine_types.h"
#include "profile.h"

typedef struct Shape
{
    Vertex_Array vertices;
    Triangle_Index_Array triangles;
} Shape;

typedef struct Shape_Assembly
{
    Vertex_Array vertices;
    Triangle_Index_Array triangles;
    Hash_Index64 vertices_hash;
} Shape_Assembly;

typedef enum Winding_Order
{
    WINDING_ORDER_CLOCKWISE,
    WINDING_ORDER_COUNTER_CLOCKWISE,
    WINDING_ORDER_INDETERMINATE, 
    //in the cases where the resulting normal is extremely close to 0
    //this heppesnt when the triangle is almost a line/point
} Winding_Order;

void shape_init(Shape* shape);
void shape_deinit(Shape* shape);
void shape_tranform(Shape* shape, Mat4 transform);

u64 vertex_hash64(Vertex vertex, u64 seed);
Shape shapes_make_unit_quad();
Shape shapes_make_unit_cube();
Shape shapes_make_cube(f32 side_size, Vec3 up_dir, Vec3 front_dir, Vec3 offset);
Shape shapes_make_quad(f32 side_size, Vec3 up_dir, Vec3 front_dir, Vec3 offset);

void shape_assembly_init(Shape_Assembly* shape, Allocator* allocator);
void shape_assembly_deinit(Shape_Assembly* shape);
void shape_assembly_init_from_shape(Shape_Assembly* assembly, Shape* shape);
void shape_assembly_copy(Shape_Assembly* out, const Shape_Assembly* in);
void shape_assembly_add_vertex(Shape_Assembly* assembly, Vertex vertex);
u32 shape_assembly_add_vertex_custom(Hash_Index64* hash, Vertex_Array* vertices, Vertex vertex);

Vec3 calculate_tangent(Vec3 edge1, Vec3 edge2, Vec2 deltaUV1, Vec2 deltaUV2);
Winding_Order calculate_winding_order(Vec3 p1, Vec3 p2, Vec3 p3, Vec3 norm1, Vec3 norm2, Vec3 norm3);

Winding_Order triangle_get_winding_order(const Vertex v1, const Vertex v2, const Vertex v3);
Winding_Order triangle_get_winding_order_at_index(const Vertex* vertices, isize vertex_count, Triangle_Index trinagle);
Vec3 triangle_calculate_tangent(const Vertex v1, const Vertex v2, const Vertex v3);
Triangle_Index triangle_set_winding_order(Winding_Order desired_order, const Vertex* vertices, isize vertex_count, Triangle_Index triangle);
void shape_set_winding_order(Winding_Order order, const Vertex* vertices, isize vertex_count, Triangle_Index* traingles, isize triangle_count);

void shapes_add_cube_sphere_side(Shape* into, isize iters, f32 radius, Vec3 side_normal, Vec3 side_front, Vec3 offset);
Shape shapes_make_cube_sphere_side(isize iters, f32 radius, Vec3 side_normal, Vec3 side_front, Vec3 offset);
Shape shapes_make_cube_sphere(isize iters, f32 radius);
Shape shapes_make_voleyball_sphere(isize iters, f32 radius);
Shape shapes_make_uv_sphere(isize iters, f32 radius);
Shape shape_make_z_looking_frusthum(f32 aspect, f32 near, f32 far, f32 fov);
Shape shape_make_frusthum(f32 aspect, f32 near, f32 far, f32 fov, Vec3 looking_at);
Shape shape_make_vertical_capsule(isize iters, f32 radius, f32 radii_distance);
Shape shape_make_capsule(isize iters, f32 radius, f32 radii_distance, Vec3 radii_delta);

extern const Vertex XZ_QUAD_VERTICES[4];
extern const Triangle_Index XZ_QUAD_INDECES[2];

extern const Vertex CUBE_VERTICES[36];
extern const Triangle_Index CUBE_INDECES[12];

#define E 0.5f
const Vertex XZ_QUAD_VERTICES[] = {
    { E, 0,  E, {1, 1}, {0, 1, 0}},
    {-E, 0,  E, {1, 0}, {0, 1, 0}}, 
    {-E, 0, -E, {0, 0}, {0, 1, 0}},
    { E, 0, -E, {0, 1}, {0, 1, 0}},
};

const Triangle_Index XZ_QUAD_INDECES[] = { 
    1, 0, 2, 0, 3, 2
};

const Vertex CUBE_VERTICES[] = {
    // Back face
    {-E, -E, -E,  0, 0, {0, 0, -1}}, // Bottom-left
    { E,  E, -E,  1, 1, {0, 0, -1}}, // top-right
    { E, -E, -E,  1, 0, {0, 0, -1}}, // bottom-right         
    { E,  E, -E,  1, 1, {0, 0, -1}}, // top-right
    {-E, -E, -E,  0, 0, {0, 0, -1}}, // bottom-left
    {-E,  E, -E,  0, 1, {0, 0, -1}}, // top-left
    // Front face
    {-E, -E,  E,  0, 0, {0, 0, 1}}, // bottom-left
    { E, -E,  E,  1, 0, {0, 0, 1}}, // bottom-right
    { E,  E,  E,  1, 1, {0, 0, 1}}, // top-right
    { E,  E,  E,  1, 1, {0, 0, 1}}, // top-right
    {-E,  E,  E,  0, 1, {0, 0, 1}}, // top-left
    {-E, -E,  E,  0, 0, {0, 0, 1}}, // bottom-left
    // Left face
    {-E,  E,  E,  1, 0, {-1, 0, 0}}, // top-right
    {-E,  E, -E,  1, 1, {-1, 0, 0}}, // top-left
    {-E, -E, -E,  0, 1, {-1, 0, 0}}, // bottom-left
    {-E, -E, -E,  0, 1, {-1, 0, 0}}, // bottom-left
    {-E, -E,  E,  0, 0, {-1, 0, 0}}, // bottom-right
    {-E,  E,  E,  1, 0, {-1, 0, 0}}, // top-right
    // Right face
    {E,  E,  E,  1, 0, {1, 0, 0}}, // top-left
    {E, -E, -E,  0, 1, {1, 0, 0}}, // bottom-right
    {E,  E, -E,  1, 1, {1, 0, 0}}, // top-right         
    {E, -E, -E,  0, 1, {1, 0, 0}}, // bottom-right
    {E,  E,  E,  1, 0, {1, 0, 0}}, // top-left
    {E, -E,  E,  0, 0, {1, 0, 0}}, // bottom-left     
    // Bottom face
    {-E, -E, -E,  0, 1, {0, -1, 0}}, // top-right
    { E, -E, -E,  1, 1, {0, -1, 0}}, // top-left
    { E, -E,  E,  1, 0, {0, -1, 0}}, // bottom-left
    { E, -E,  E,  1, 0, {0, -1, 0}}, // bottom-left
    {-E, -E,  E,  0, 0, {0, -1, 0}}, // bottom-right
    {-E, -E, -E,  0, 1, {0, -1, 0}}, // top-right
    // Top face
    {-E,  E, -E,  0, 1, {0, 1, 0}}, // top-left
    { E,  E,  E,  1, 0, {0, 1, 0}}, // bottom-right
    { E,  E, -E,  1, 1, {0, 1, 0}}, // top-right     
    { E,  E,  E,  1, 0, {0, 1, 0}}, // bottom-right
    {-E,  E, -E,  0, 1, {0, 1, 0}}, // top-left
    {-E,  E,  E,  0, 0, {0, 1, 0}}  // bottom-left        
};

#undef E

const Triangle_Index CUBE_INDECES[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35
};

void shape_deinit(Shape* shape)
{
    array_deinit(&shape->vertices);
    array_deinit(&shape->triangles);
}

void shape_assembly_init(Shape_Assembly* shape, Allocator* allocator)
{
    array_init(&shape->vertices, allocator);
    array_init(&shape->triangles, allocator);
    hash_index64_init(&shape->vertices_hash, allocator);
}

void shape_assembly_deinit(Shape_Assembly* shape)
{
    array_deinit(&shape->vertices);
    array_deinit(&shape->triangles);
    hash_index64_deinit(&shape->vertices_hash);
}
void shape_assembly_copy(Shape_Assembly* out, const Shape_Assembly* in)
{
    array_copy(&out->triangles, in->triangles);
    array_copy(&out->vertices, in->vertices);
    hash_index64_copy(&out->vertices_hash, in->vertices_hash);
}

void shape_tranform(Shape* shape, Mat4 transform)
{
    PERF_COUNTER_START(c);

    Mat4 normal_matrix = mat4_inverse_nonuniform_scale(transform);

    for(isize i = 0; i < shape->vertices.size; i++)
    {
        Vertex* vertex = &shape->vertices.data[i];
        vertex->pos = mat4_apply(transform, vertex->pos);
        vertex->norm = mat4_mul_vec3(normal_matrix, vertex->norm);
    }

    PERF_COUNTER_END(c);
}

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
    array_append(&out.triangles, XZ_QUAD_INDECES, index_count);

    return out;
}

//makes a cube in spanning [-0.5, 0.5]^3 centered at origin
Shape shapes_make_unit_cube()
{
    
    Shape out = {0};
    isize vertex_count = STATIC_ARRAY_SIZE(CUBE_VERTICES);
    isize index_count = STATIC_ARRAY_SIZE(CUBE_INDECES);
    array_append(&out.vertices, CUBE_VERTICES, vertex_count);
    array_append(&out.triangles, CUBE_INDECES, index_count);

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


Winding_Order triangle_get_winding_order(const Vertex v1, const Vertex v2, const Vertex v3)
{
    Vec3 avg_norm = vec3_add(vec3_add(v1.norm, v2.norm), v3.norm);
    avg_norm = vec3_norm(avg_norm);

    Vec3 edge1 = vec3_sub(v2.pos, v1.pos);
    Vec3 edge2 = vec3_sub(v3.pos, v1.pos);

    Vec3 triangle_right_norm = vec3_cross(edge1, edge2);
    f32 similarity = vec3_dot(avg_norm, triangle_right_norm);
    
    if(is_near_scaledf(similarity, 0, EPSILON))
        return WINDING_ORDER_INDETERMINATE;
    if(similarity < 0)
        return WINDING_ORDER_COUNTER_CLOCKWISE;
    else
        return WINDING_ORDER_CLOCKWISE;
}

void dummy_func() {}

Winding_Order triangle_get_winding_order_at_index(const Vertex* vertices, isize vertex_count, Triangle_Index trinagle)
{
    !dummy_func ? (void) &vertex_count : (void) 0;

    CHECK_BOUNDS(trinagle.vertex_i[0], vertex_count);
    CHECK_BOUNDS(trinagle.vertex_i[1], vertex_count);
    CHECK_BOUNDS(trinagle.vertex_i[2], vertex_count);

    Vertex v1 = vertices[trinagle.vertex_i[0]];
    Vertex v2 = vertices[trinagle.vertex_i[1]];
    Vertex v3 = vertices[trinagle.vertex_i[2]];

    return triangle_get_winding_order(v1, v2, v3);
}

u32 shape_assembly_add_vertex_custom(Hash_Index64* hash, Vertex_Array* vertices, Vertex vertex)
{
    PERF_COUNTER_START(c);
    u64 hashed = vertex_hash64(vertex, 0);

    //The index of a new vertex if it was to be inserted
    isize inserted_i = vertices->size;
    isize found = hash_index64_find_or_insert(hash, hashed, (u64) inserted_i);
    isize entry = (isize) hash->entries[found].value;

    //If we just inserted add it to the array.
    if(entry == inserted_i)
    {
        array_push(vertices, vertex);
    }
    else
    {
        //If it is the right one and return its index.
        //We still check for exact equality of the vertex. It will almost always
        //succeed because the chances of hash colision are astronomically low.
        CHECK_BOUNDS(entry, vertices->size);
        Vertex found_vertex = vertices->data[entry];
        bool is_equal = memcmp(&vertex, &found_vertex, sizeof found_vertex) == 0;
        if(is_equal == false)
        {
            //if hash collision add the hash again.
            PERF_COUNTER_START(hash_collisions);
            hash_index64_insert(hash, hashed, inserted_i);
            array_push(vertices, vertex);
            entry = inserted_i;
            PERF_COUNTER_END(hash_collisions);
        }
    }
    
    CHECK_BOUNDS(entry, vertices->size);
    PERF_COUNTER_END(c);
    return (u32) entry;

}


Vec3 calculate_tangent(Vec3 edge1, Vec3 edge2, Vec2 deltaUV1, Vec2 deltaUV2)
{
    Vec3 tangent = {0};
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

    return tangent;
}

Vec3 triangle_calculate_tangent(const Vertex v1, const Vertex v2, const Vertex v3)
{
    Vec3 edge1 = vec3_sub(v2.pos, v1.pos);
    Vec3 edge2 = vec3_sub(v3.pos, v1.pos);
    Vec2 deltaUV1 = vec2_sub(v2.uv, v1.uv);
    Vec2 deltaUV2 = vec2_sub(v3.uv, v1.uv);  

    return calculate_tangent(edge1, edge2, deltaUV1, deltaUV2);
}

Triangle_Index triangle_set_winding_order(Winding_Order desired_order, const Vertex* vertices, isize vertex_count, Triangle_Index triangle)
{
    Winding_Order current_order = triangle_get_winding_order_at_index(vertices, vertex_count, triangle);
    Triangle_Index out_triangle = triangle;
    if(current_order != desired_order)
        SWAP(&out_triangle.vertex_i[0], &out_triangle.vertex_i[1], u32); 
    
    return out_triangle;
}

void shape_set_winding_order(Winding_Order order, const Vertex* vertices, isize vertex_count, Triangle_Index* traingles, isize triangle_count)
{
    for(isize i = 0; i < triangle_count; i++)
        traingles[i] = triangle_set_winding_order(order, vertices, vertex_count, traingles[i]);
}


//@TODO:
//Smooths all triangle normals that have angle between them higher than smooth_from_angle. Might have to add additional vertices.
//Uses smoothing_factor to determine how much influence the adjecent vertices have on the resulting normal. 
//the smooth_from_angle refers to the angle between the faces from inside the geometry in a convex shape. 
//Or put other way between corners of a box smooth_from_angle is 90 degrees nto 270. (this means if from angle is 0 all edges will be smoothed out)
//void shape_assembly_adjust_normals(Hash_Index64* hash, Vertex_Array* vertices, Triangle_Index* traingles, isize triangle_count, f32 smooth_from_angle, f32 smoothing_factor, f32 sharpen_to_angle, f32 sharpen_factor)
//{
//    
//}



//@TODO: change iters on sphere creation to more sophisticated budget mechanisms
typedef struct Face_Budget
{
    f32 max_size;
    i32 max_vertices;
    i32 max_faces;
} Face_Budget;

void shapes_add_cube_sphere_side(Shape* into, isize iters, f32 radius, Vec3 side_normal, Vec3 side_front, Vec3 offset)
{
    if(iters <= 0)
        return;
        
    PERF_COUNTER_START(c);
    isize vertices_before = into->vertices.size;
    isize indeces_before = into->triangles.size;

    const Vec3 origin = vec3(0, 0, 0);
    const Vec3 corners[4] = {
        vec3(-1, 1, -1),
        vec3(1, 1, -1),
        vec3(-1, 1, 1),
        vec3(1, 1, 1),
    };

    const f32 y_angle = vec3_angle_between(vec3_sub(corners[0], origin), vec3_sub(corners[1], origin));
    const f32 x_angle = vec3_angle_between(vec3_sub(corners[2], origin), vec3_sub(corners[3], origin));
    
    array_reserve(&into->triangles, indeces_before + (iters) * (iters) * 2);
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
            Triangle_Index tri1 = {0}; 
            tri1.vertex_i[0] = (y + 1) * (uiters + 1) + x + ubefore;
            tri1.vertex_i[1] = y       * (uiters + 1) + x + ubefore;
            tri1.vertex_i[2] = y       * (uiters + 1) + x + 1 + ubefore;
            
            Triangle_Index tri2 = {0};
            tri2.vertex_i[0] = (y + 1) * (uiters + 1) + x + ubefore;
            tri2.vertex_i[1] = y       * (uiters + 1) + x + 1 + ubefore;
            tri2.vertex_i[2] = (y + 1) * (uiters + 1) + x + 1 + ubefore;

            array_push(&into->triangles, tri1);
            array_push(&into->triangles, tri2);
            
            ASSERT(triangle_get_winding_order_at_index(into->vertices.data, into->vertices.size, tri1) == WINDING_ORDER_CLOCKWISE);
            ASSERT(triangle_get_winding_order_at_index(into->vertices.data, into->vertices.size, tri2) == WINDING_ORDER_CLOCKWISE);
        }
        
    }
    PERF_COUNTER_END(c);
}

Shape shapes_make_cube_sphere_side(isize iters, f32 radius, Vec3 side_normal, Vec3 side_front, Vec3 offset)
{
    Shape out = {0};
    shapes_add_cube_sphere_side(&out, iters, radius, side_normal, side_front, offset);
    return out;
}

Shape shapes_make_cube_sphere(isize iters, f32 radius)
{
    PERF_COUNTER_START(c);
    Shape out = {0};
    array_reserve(&out.triangles, (iters) * (iters) * 2 * 6);
    array_reserve(&out.vertices, (iters + 1) * (iters + 1) * 6);

    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 1, 0), vec3(1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(1, 0, 0), vec3(0, 0, 1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, 1), vec3(1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, -1, 0), vec3(-1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(-1, 0, 0), vec3(0, 0, -1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, -1), vec3(-1, 0, 0), vec3_of(0));
    
    PERF_COUNTER_END(c);
    return out;
}

Shape shapes_make_voleyball_sphere(isize iters, f32 radius)
{
    PERF_COUNTER_START(c);
    Shape out = {0};
    array_reserve(&out.triangles, (iters) * (iters) * 2 * 6);
    array_reserve(&out.vertices, (iters + 1) * (iters + 1) * 6);

    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 1, 0), vec3(1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(1, 0, 0), vec3(0, 0, 1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, 1), vec3(0, 1, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, -1, 0), vec3(-1, 0, 0), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(-1, 0, 0), vec3(0, 0, -1), vec3_of(0));
    shapes_add_cube_sphere_side(&out, iters, radius, vec3(0, 0, -1), vec3(0, -1, 0), vec3_of(0));
    
    PERF_COUNTER_END(c);
    return out;
}

Shape shapes_make_uv_sphere(isize iters, f32 radius)
{
    (void) radius;
    PERF_COUNTER_START(c);
    ASSERT(radius > 0);
    ASSERT(iters >= 0);

    //add the initial triangles
    Vertex_Array vertices = {0};
    Triangle_Index_Array triangles = {0};

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
            Triangle_Index tri1 = {0}; 
            tri1.vertex_i[0] = (y + 1) * (x_segments + 1) + x;
            tri1.vertex_i[1] = y       * (x_segments + 1) + x;
            tri1.vertex_i[2] = y       * (x_segments + 1) + x + 1;
            
            Triangle_Index tri2 = {0};
            tri2.vertex_i[0] = (y + 1) * (x_segments + 1) + x;
            tri2.vertex_i[1] = y       * (x_segments + 1) + x + 1;
            tri2.vertex_i[2] = (y + 1) * (x_segments + 1) + x + 1;
            array_push(&triangles, tri1);
            array_push(&triangles, tri2);

            #ifdef DO_ASSERTS
            // Vertices must normally have a WINDING_ORDER_CLOCKWISE order, however
            // when the triangle is degenerate (almost line, or point) their order might be WINDING_ORDER_INDETERMINATE
            // We assert this.
            
            Triangle_Index tris[2] = {tri1, tri2};
            for(isize i = 0; i < 2; i++)
            {
                Vertex v1 = vertices.data[tris[i].vertex_i[0]];
                Vertex v2 = vertices.data[tris[i].vertex_i[1]];
                Vertex v3 = vertices.data[tris[i].vertex_i[2]];

                Winding_Order order = triangle_get_winding_order(v1, v2, v3);
                if(order != WINDING_ORDER_CLOCKWISE)
                {
                    bool are_near = vec3_is_near(v1.pos, v2.pos, EPSILON)
                        || vec3_is_near(v1.pos, v3.pos, EPSILON)
                        || vec3_is_near(v2.pos, v3.pos, EPSILON);

                    ASSERT(are_near);
                    ASSERT(order == WINDING_ORDER_INDETERMINATE);
                }
                else
                {
                    ASSERT(order == WINDING_ORDER_CLOCKWISE);
                }
            }
            
            #endif
        }
        
    }

    out_shape.triangles = triangles;
    out_shape.vertices = vertices;
    PERF_COUNTER_END(c);
    return out_shape;
}
