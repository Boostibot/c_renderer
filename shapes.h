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

void mesh_deinit(Shape* mesh)
{
    array_deinit(&mesh->vertices);
    array_deinit(&mesh->indeces);
}

#define E 1.0f
const Vertex XZ_QUAD_VERTICES[] = {
     E, 0,  E, {1, 1}, {0, 1, 0},
    -E, 0,  E, {0, 1}, {0, 1, 0}, 
    -E, 0, -E, {0, 0}, {0, 1, 0},
     E, 0, -E, {1, 0}, {0, 1, 0},
};

const Vertex XY_QUAD_VERTICES[] = { 
     E,  E, 0.0f, 1.0f, 1.0f, {0},
    -E,  E, 0.0f, 0.0f, 1.0f, {0},
    -E, -E, 0.0f, 0.0f, 0.0f, {0},
     E, -E, 0.0f, 1.0f, 0.0f, {0},
     E,  E, 0.0f, 1.0f, 1.0f, {0},
    -E, -E, 0.0f, 0.0f, 0.0f, {0},
};

const Vertex YZ_QUAD_VERTICES[] = {
    0.0f,  E,  E, 1.0f, 1.0f, {0},
    0.0f, -E,  E, 0.0f, 1.0f, {0},
    0.0f, -E, -E, 0.0f, 0.0f, {0},
    0.0f,  E, -E, 1.0f, 0.0f, {0},
    0.0f,  E,  E, 1.0f, 1.0f, {0},
    0.0f, -E, -E, 0.0f, 0.0f, {0},
};

const Triangle_Indeces XZ_QUAD_INDECES[] = { 
    1, 0, 2, 0, 3, 2
};

const Triangle_Indeces XY_QUAD_INDECES[] = { 
    0, 1, 2, 3, 4, 5
};

const Triangle_Indeces YZ_QUAD_INDECES[] = { 
    0, 1, 2, 3, 4, 5
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
    isize vertex_count = STATIC_ARRAY_SIZE(XZ_QUAD_VERTICES);
    isize index_count = STATIC_ARRAY_SIZE(XZ_QUAD_INDECES);
    array_append(&out.vertices, XZ_QUAD_VERTICES, vertex_count);
    array_append(&out.indeces, XZ_QUAD_INDECES, index_count);

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

INTERNAL u32 _shape_assembly_add_vertex(Hash_Index* hash, Vertex_Array* vertices, Vertex vertex)
{
    u64 hashed = vertex_hash64(vertex, 0);

    //try to exactly find it in the hash. will almost always iterate only once.
    // the chance of hash colision are astronomically low.
    isize found = hash_index_find(*hash, hashed);
    while(found != -1)
    {
        isize entry = hash->entries[found].value;
        Vertex found_vertex = vertices->data[entry];
        bool is_equal = memcpy(&vertex, &found_vertex, sizeof found_vertex);
        if(is_equal)
        {
            return (u32) entry;
        }
        else
        {
            isize finished_at = 0;
            found = hash_index_find_next(*hash, hashed, found, &finished_at);
        }
    }

    array_push(vertices, vertex);
    u32 inserted_i = (u32) (vertices->size - 1);
    hash_index_insert(hash, hashed, inserted_i);
    return inserted_i;
}

Vertex vertex_lerp(Vertex low, Vertex high, f32 t)
{
    Vertex out = {0};
    out.pos = vec3_lerp(low.pos, high.pos, t);
    out.uv = vec2_lerp(low.uv, high.uv, t);
    return out;
}

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
        vertex->norm = mat4_apply(normal_matrix, vertex->norm);
    }
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

//true - CW
//false - CCW
bool check_winding_order(Vertex v1, Vertex v2, Vertex v3)
{
    Vec3 avg_norm = vec3_add(vec3_add(v1.norm, v2.norm), v3.norm);
    avg_norm = vec3_norm(avg_norm);

    Vec3 edge1 = vec3_sub(v2.pos, v1.pos);
    Vec3 edge2 = vec3_sub(v3.pos, v1.pos);

    Vec3 triangle_right_norm = vec3_cross(edge1, edge2);
    f32 similarity = vec3_dot(avg_norm, triangle_right_norm);

    if(similarity < 0)
        return false;
    else
        return true;
}

bool check_winding_order_index(Vertex* vertices, Triangle_Indeces trinagle)
{
    Vertex v1 = vertices[trinagle.vertex_i[0]];
    Vertex v2 = vertices[trinagle.vertex_i[1]];
    Vertex v3 = vertices[trinagle.vertex_i[2]];

    return check_winding_order(v1, v2, v3);
}

//returns the xz sixth of a sphere (curved upwards) properly uv mapped to [0, 1]^2. 
Shape shapes_make_xz_sphere_side(isize iters, f32 radius, f32 p)
{
    isize vertex_budget = 0;
    isize face_budget = 0;
    f32 face_size_budget = 0;
    (void) vertex_budget;
    (void) face_budget;
    (void) face_size_budget;
    (void) radius;

    const Triangle_Indeces* quad_indices = XZ_QUAD_INDECES;
    const Vertex* quad_vertices = XZ_QUAD_VERTICES;
    isize quad_indices_count = STATIC_ARRAY_SIZE(XZ_QUAD_INDECES);
    
    //@TODO: refactor to use loops of 3
    Allocator* default_alloc = allocator_get_default();
    Allocator* scratch_alloc = allocator_get_scratch();

    //add the initial indeces
    Vertex_Array vertices = {default_alloc};
    Triangle_Indeces_Array indeces1 = {default_alloc};
    Triangle_Indeces_Array indeces2 = {default_alloc};
    Hash_Index hash = {scratch_alloc};
    
    isize needed_vertices = 4;
    isize needed_trinagles = 4;
    for(isize k = 0; k < iters; k++)
    {
        needed_vertices *= 2;
        needed_trinagles *= 4;
    }
    hash_index_reserve(&hash, needed_trinagles);
    array_reserve(&indeces1, needed_vertices);
    array_reserve(&indeces2, needed_vertices);

    for(isize i = 0; i < quad_indices_count; i++)
    {
        Triangle_Indeces triangle = quad_indices[i];
        Vertex triangle_vertices[3] = {0};
        triangle_vertices[0] = quad_vertices[triangle.vertex_i[0]];
        triangle_vertices[1] = quad_vertices[triangle.vertex_i[1]];
        triangle_vertices[2] = quad_vertices[triangle.vertex_i[2]];

        ASSERT(check_winding_order(triangle_vertices[0], triangle_vertices[1], triangle_vertices[2]));

        //transform each vertex so that it lays on the unit sphere
        for(isize j = 0; j < 3; j++)
        {
            Vertex* vertex = &triangle_vertices[j];
            vertex->pos.y = E;
            vertex->pos = vec3_scale(vec3_p_norm(vertex->pos, p), radius);
            vertex->norm = vertex->pos;
        }

        Triangle_Indeces inserted_triangle = {0};
        inserted_triangle.vertex_i[0] = _shape_assembly_add_vertex(&hash, &vertices, triangle_vertices[0]);
        inserted_triangle.vertex_i[1] = _shape_assembly_add_vertex(&hash, &vertices, triangle_vertices[1]);
        inserted_triangle.vertex_i[2] = _shape_assembly_add_vertex(&hash, &vertices, triangle_vertices[2]);

        ASSERT(inserted_triangle.vertex_i[0] < vertices.size);
        ASSERT(inserted_triangle.vertex_i[1] < vertices.size);
        ASSERT(inserted_triangle.vertex_i[2] < vertices.size);

        array_push(&indeces1, inserted_triangle);
    }
    
    Triangle_Indeces_Array* in_indeces = &indeces2;
    Triangle_Indeces_Array* out_indeces = &indeces1;
    for(isize k = 0; k < iters; k++)
    {
        if(k % 2 == 0)
        {
            in_indeces = &indeces1;
            out_indeces = &indeces2;
        }
        else
        {
            in_indeces = &indeces2;
            out_indeces = &indeces1;
        }
        
        array_clear(out_indeces);

        isize size_before = in_indeces->size;
        for(isize i = 0; i < size_before; i++)
        {
            Triangle_Indeces triangle = in_indeces->data[i];
            Vertex triangle_vertices[3] = {0};

            triangle_vertices[0] = *array_get(vertices, triangle.vertex_i[0]);
            triangle_vertices[1] = *array_get(vertices, triangle.vertex_i[1]);
            triangle_vertices[2] = *array_get(vertices, triangle.vertex_i[2]);
            
            ASSERT(check_winding_order(triangle_vertices[0], triangle_vertices[1], triangle_vertices[2]));

            Vertex mids[3] = {0};
            //@TODO: maybe add norm to lerp?
            mids[0] = vertex_lerp(triangle_vertices[0], triangle_vertices[1], 0.5f);
            mids[1] = vertex_lerp(triangle_vertices[1], triangle_vertices[2], 0.5f);
            mids[2] = vertex_lerp(triangle_vertices[2], triangle_vertices[0], 0.5f);
             
            mids[0].pos = vec3_scale(vec3_p_norm(mids[0].pos, p), radius);
            mids[1].pos = vec3_scale(vec3_p_norm(mids[1].pos, p), radius);
            mids[2].pos = vec3_scale(vec3_p_norm(mids[2].pos, p), radius);
            //mids[0].pos = vec3_norm(mids[0].pos);
            //mids[1].pos = vec3_norm(mids[1].pos);
            //mids[2].pos = vec3_norm(mids[2].pos);
            
            mids[0].norm = mids[0].pos;
            mids[1].norm = mids[1].pos;
            mids[2].norm = mids[2].pos;


            u32 m0 = _shape_assembly_add_vertex(&hash, &vertices, mids[0]);
            u32 m1 = _shape_assembly_add_vertex(&hash, &vertices, mids[1]);
            u32 m2 = _shape_assembly_add_vertex(&hash, &vertices, mids[2]);

            Triangle_Indeces corner1 = {triangle.vertex_i[0], m0, m2};
            Triangle_Indeces corner2 = {triangle.vertex_i[1], m1, m0};
            Triangle_Indeces corner3 = {triangle.vertex_i[2], m2, m1};
            Triangle_Indeces central = {m0, m1, m2};

            array_push(out_indeces, corner1);
            array_push(out_indeces, corner2);
            array_push(out_indeces, corner3);
            array_push(out_indeces, central);
        }

        ASSERT(size_before == in_indeces->size);
    }

    array_deinit(in_indeces);
    hash_index_deinit(&hash);

    Shape out_shape = {0};
    out_shape.indeces = *out_indeces;
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
#undef E