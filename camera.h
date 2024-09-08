#pragma once
#include "lib/math.h"
#include "lib/defines.h"

typedef struct Camera
{
    f32 fov;
    f32 aspect_ratio;
    Vec3 pos;
    Vec3 looking_at;
    Vec3 up_dir;

    f32 near;
    f32 far;
    
    //if is set to true looking_at is relative to pos 
    //effectively making the looking_at become looking_at + pos
    bool is_position_relative;  

    //specifies if should use ortographic projection.
    //If is true uses top/bot/left/right. The area of the rectangle must not be 0!
    bool is_ortographic;  
    bool _[2];

    f32 top;
    f32 bot;
    f32 left;
    f32 right;
} Camera;

Mat4 camera_make_projection_matrix(Camera camera)
{
    Mat4 projection = {0};
    if(camera.is_ortographic)
        projection = mat4_ortographic_projection(camera.bot, camera.top, camera.left, camera.right, camera.near, camera.far);
    else
        projection = mat4_perspective_projection(camera.fov, camera.aspect_ratio, camera.near, camera.far);
    return projection;
}

Vec3 camera_get_look_dir(Camera camera)
{
    Vec3 look_dir = camera.looking_at;
    if(camera.is_position_relative == false)
        look_dir = vec3_sub(look_dir, camera.pos);
    return look_dir;
}

Vec3 camera_get_looking_at(Camera camera)
{
    Vec3 looking_at = camera.looking_at;
    if(camera.is_position_relative)
        looking_at = vec3_add(looking_at, camera.pos); 
    return looking_at;
}

Mat4 camera_make_view_matrix(Camera camera)
{
    Vec3 looking_at = camera_get_looking_at(camera);
    Mat4 view = mat4_look_at(camera.pos, looking_at, camera.up_dir);
    return view;
}