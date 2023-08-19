#pragma once

#include "math.h"
#include "array.h"
#include "hash_index.h"
#include "hash.h"

typedef struct Vertex
{
    Vec3 pos;
    Vec2 uv;
} Vertex;

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

void mesh_deinit(Shape* mesh)
{
    array_deinit(&mesh->vertices);
    array_deinit(&mesh->indeces);
}


#define E 1.0f
const Vertex QUAD_VERTICES[] = {
    -E,  0.0f, -E,  0.0f, 1.0f,
     E,  0.0f,  E,  1.0f, 0.0f,
     E,  0.0f, -E,  1.0f, 1.0f,
     E,  0.0f,  E,  1.0f, 0.0f,
    -E,  0.0f, -E,  0.0f, 1.0f,
    -E,  0.0f,  E,  0.0f, 0.0f 
};

const Triangle_Indeces QUAD_INDECES[] = { 
    0, 1, 2, 3, 4, 5
};

const Vertex CUBE_VERTICES[] = {
    // Back face
    -E, -E, -E,  0.0f, 0.0f, // Bottom-left
     E,  E, -E,  1.0f, 1.0f, // top-right
     E, -E, -E,  1.0f, 0.0f, // bottom-right         
     E,  E, -E,  1.0f, 1.0f, // top-right
    -E, -E, -E,  0.0f, 0.0f, // bottom-left
    -E,  E, -E,  0.0f, 1.0f, // top-left
    // Front face
    -E, -E,  E,  0.0f, 0.0f, // bottom-left
     E, -E,  E,  1.0f, 0.0f, // bottom-right
     E,  E,  E,  1.0f, 1.0f, // top-right
     E,  E,  E,  1.0f, 1.0f, // top-right
    -E,  E,  E,  0.0f, 1.0f, // top-left
    -E, -E,  E,  0.0f, 0.0f, // bottom-left
    // Left face
    -E,  E,  E,  1.0f, 0.0f, // top-right
    -E,  E, -E,  1.0f, 1.0f, // top-left
    -E, -E, -E,  0.0f, 1.0f, // bottom-left
    -E, -E, -E,  0.0f, 1.0f, // bottom-left
    -E, -E,  E,  0.0f, 0.0f, // bottom-right
    -E,  E,  E,  1.0f, 0.0f, // top-right
    // Right face
     E,  E,  E,  1.0f, 0.0f, // top-left
     E, -E, -E,  0.0f, 1.0f, // bottom-right
     E,  E, -E,  1.0f, 1.0f, // top-right         
     E, -E, -E,  0.0f, 1.0f, // bottom-right
     E,  E,  E,  1.0f, 0.0f, // top-left
     E, -E,  E,  0.0f, 0.0f, // bottom-left     
    // Bottom face
    -E, -E, -E,  0.0f, 1.0f, // top-right
     E, -E, -E,  1.0f, 1.0f, // top-left
     E, -E,  E,  1.0f, 0.0f, // bottom-left
     E, -E,  E,  1.0f, 0.0f, // bottom-left
    -E, -E,  E,  0.0f, 0.0f, // bottom-right
    -E, -E, -E,  0.0f, 1.0f, // top-right
    // Top face
    -E,  E, -E,  0.0f, 1.0f, // top-left
     E,  E,  E,  1.0f, 0.0f, // bottom-right
     E,  E, -E,  1.0f, 1.0f, // top-right     
     E,  E,  E,  1.0f, 0.0f, // bottom-right
    -E,  E, -E,  0.0f, 1.0f, // top-left
    -E,  E,  E,  0.0f, 0.0f  // bottom-left        
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

//makes a quad in spanning x:[-0.5, 0.5] y:{0} z:[-0.5, 0.5] centered at origin
Shape shapes_make_xz_quad()
{
    Shape out = {0};
    isize vertex_count = STATIC_ARRAY_SIZE(QUAD_VERTICES);
    isize index_count = STATIC_ARRAY_SIZE(QUAD_INDECES);
    array_append(&out.vertices, QUAD_VERTICES, vertex_count);
    array_append(&out.indeces, QUAD_INDECES, index_count);

    return out;
}

//makes a cube in spanning [-0.5, 0.5]^3 centered at origin
Shape shapes_make_cube()
{
    Shape out = {0};
    isize vertex_count = STATIC_ARRAY_SIZE(CUBE_VERTICES);
    isize index_count = STATIC_ARRAY_SIZE(CUBE_INDECES);
    array_append(&out.vertices, CUBE_VERTICES, vertex_count);
    array_append(&out.indeces, CUBE_INDECES, index_count);

    return out;
}



INTERNAL u32 _shape_assembly_add_vertex(Hash_Index* hash, Shape* mesh, Vertex vertex)
{
    u64 hashed = vertex_hash64(vertex, 0);

    //try to exactly find it in the hash. will almost always iterate only once.
    // the chance of hash colision are astronomically low.
    isize found = hash_index_find(*hash, hashed);
    while(found != -1)
    {
        isize entry = hash->entries[found].value;
        Vertex found_vertex = mesh->vertices.data[entry];
        bool is_equal = memcpy(&vertex, &found_vertex, sizeof found_vertex);
        if(is_equal)
        {
            return (u32) found;
        }
        else
        {
            isize finished_at = 0;
            found = hash_index_find_next(*hash, hashed, found, &finished_at);
        }
    }

    array_push(&mesh->vertices, vertex);
    return (u32) (mesh->vertices.size - 1);
}

Vertex vertex_lerp(Vertex low, Vertex high, f32 t)
{
    Vertex out = {0};
    out.pos = vec3_lerp(low.pos, high.pos, t);
    out.uv = vec2_lerp(low.uv, high.uv, t);
    return out;
}

//returns the xz sixth of a sphere (curved upwards) properly uv mapped to [0, 1]^2. 
Shape shapes_make_xz_sphere_side(isize vertex_budget, isize face_budget, f32 face_size_budget, f32 radius)
{
    Shape quad_shape = shapes_make_xz_quad();

    (void) vertex_budget;
    (void) face_budget;
    (void) face_size_budget;

    f32 dist_to_corner = sqrtf(3.0f);
    f32 corner_dist = 1.0f/dist_to_corner*radius;
    f32 C = corner_dist;

    Vertex upper_quad_vertices[] = {
         C, C,  C, 1.0f, 1.0f,
        -C, C,  C, 0.0f, 1.0f,
         C, C, -C, 1.0f, 0.0f,
        -C, C, -C, 0.0f, 0.0f,
    };

    Triangle_Indeces upper_quad_indeces[] = {
        0, 1, 2, 
        1, 2, 3
    };
    (void) upper_quad_vertices;
    (void) upper_quad_indeces;
    

    for(isize i = 0; i < quad_shape.vertices.size; i++)
    {
        Vertex* vertex = &quad_shape.vertices.data[i];
        vertex->pos.x *= corner_dist;
        vertex->pos.y = corner_dist;
        vertex->pos.z *= corner_dist;

        ASSERT(vec3_len(vertex->pos) == radius);
    }
    
    Shape out_shape = {0};
    Hash_Index hash = {allocator_get_scratch()};
    
    //add the initial indeces
    for(isize i = 0; i < quad_shape.indeces.size; i++)
    {
        Triangle_Indeces triangle = quad_shape.indeces.data[i];
        Vertex triangle_vertices[3] = {0};

        triangle_vertices[0] = *array_get(quad_shape.vertices, triangle.vertex_i[0]);
        triangle_vertices[1] = *array_get(quad_shape.vertices, triangle.vertex_i[1]);
        triangle_vertices[2] = *array_get(quad_shape.vertices, triangle.vertex_i[2]);

        Triangle_Indeces inserted_triangle = {0};
        inserted_triangle.vertex_i[0] = _shape_assembly_add_vertex(&hash, &out_shape, triangle_vertices[0]);
        inserted_triangle.vertex_i[1] = _shape_assembly_add_vertex(&hash, &out_shape, triangle_vertices[1]);
        inserted_triangle.vertex_i[2] = _shape_assembly_add_vertex(&hash, &out_shape, triangle_vertices[2]);

        array_push(&out_shape.indeces, inserted_triangle);
    }

    for(isize i = 0; i < quad_shape.indeces.size; i++)
    {
        Triangle_Indeces triangle = quad_shape.indeces.data[i];
        Vertex triangle_vertices[3] = {0};

        triangle_vertices[0] = *array_get(quad_shape.vertices, triangle.vertex_i[0]);
        triangle_vertices[1] = *array_get(quad_shape.vertices, triangle.vertex_i[1]);
        triangle_vertices[2] = *array_get(quad_shape.vertices, triangle.vertex_i[2]);

        Vertex mids[3] = {0};
        mids[0] = vertex_lerp(triangle_vertices[0], triangle_vertices[1], 0.5f);
        mids[1] = vertex_lerp(triangle_vertices[1], triangle_vertices[2], 0.5f);
        mids[2] = vertex_lerp(triangle_vertices[2], triangle_vertices[0], 0.5f);

        u32 m0 = _shape_assembly_add_vertex(&hash, &out_shape, triangle_vertices[0]);
        u32 m1 = _shape_assembly_add_vertex(&hash, &out_shape, triangle_vertices[1]);
        u32 m2 = _shape_assembly_add_vertex(&hash, &out_shape, triangle_vertices[2]);

        Triangle_Indeces corner1 = {triangle.vertex_i[0], m2, m0};
        Triangle_Indeces corner2 = {triangle.vertex_i[1], m0, m1};
        Triangle_Indeces corner3 = {triangle.vertex_i[2], m1, m2};
        Triangle_Indeces central = {m2, m1, m0};

        array_push(&out_shape.indeces, corner1);
        array_push(&out_shape.indeces, corner2);
        array_push(&out_shape.indeces, corner3);
        array_push(&out_shape.indeces, central);
    }

    return out_shape;
}


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
