#pragma once

#include "parse.h"
#include "format_lpf.h"
#include "format.h"
#include "math.h"
#include "image.h"
#include "resources.h"

typedef enum Read_Or_Write {
    SERIALIZE_READ = 0,
    SERIALIZE_WRITE = 1,
} Read_Or_Write;

typedef struct Searialize_Options {
    bool do_log;
    Read_Or_Write action;
    Allocator* allocator;
} Searialize_Options;

// TEX {
//  
// }
// 

void lpf_dyn_entry_set_value(Lpf_Dyn_Entry* entry, String type, String value)
{
    Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
    readable.value = value;
    readable.type = type;
    Lpf_Dyn_Entry created = lpf_dyn_entry_from_entry(readable, entry->allocator);
    lpf_dyn_entry_deinit(entry);
    *entry = created;
}

bool serialize_f32(Lpf_Dyn_Entry* entry, f32* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        isize index = 0;
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        return match_decimal_f32(readable.value, &index, val);
    }
    else
        lpf_dyn_entry_set_value(entry, STRING("f"), format_ephemeral("%f", *val));
    return true;
}

bool serialize_string(Lpf_Dyn_Entry* entry, String_Builder* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        builder_assign(val, readable.value);
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("s"), string_from_builder(*val));
    }
    return true;
}

bool serialize_name(Lpf_Dyn_Entry* entry, String_Builder* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        builder_assign(val, readable.value);
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("n"), string_from_builder(*val));
    }
    return true;
}

bool serialize_atom(Lpf_Dyn_Entry* entry, isize* atom_index, String type, const String* atoms, isize atoms_size, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        String just_value = string_trim_whitespace(readable.value);


        for(isize i = 0; i < atoms_size; i++)
        {
            String atom = atoms[i];
            if(string_is_equal(just_value, atom))
            {
                *atom_index = i;
                return true;
            }
        }

        *atom_index = 0;
        return false;
    }
    else
    {
        String value = atoms[*atom_index];
        lpf_dyn_entry_set_value(entry, type, value);
        return true;
    }
}

bool serialize_bool(Lpf_Dyn_Entry* entry, bool* val, Searialize_Options options)
{
    const String atoms[] = {
        STRING("true"),
        STRING("false")
    };

    isize atom_index = *val;
    bool state = serialize_atom(entry, &atom_index, STRING("b"), atoms, STATIC_ARRAY_SIZE(atoms), options);
    *val = (bool) atom_index;

    return state;
}

bool serialize_bool(Lpf_Dyn_Entry* entry, bool* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        String just_value = string_trim_whitespace(readable.value);

        bool state = true;
        if(string_is_equal(just_value, STRING("true")))
            *val = true;
        else if(string_is_equal(just_value, STRING("false")))
            *val = false;
        else
            state = false;

        return state;
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("f"), *val ? STRING("true") : STRING("false"));
        return true;
    }
}

bool serialize_f64(Lpf_Dyn_Entry* entry, f64* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        isize index = 0;
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        return match_decimal_f64(readable.value, &index, val);
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("f"), format_ephemeral("%lf", *val));
        return true;
    }
}

bool serialize_i64(Lpf_Dyn_Entry* entry, i64* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        isize index = 0;
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        return match_decimal_i64(readable.value, &index, val);
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("i"), format_ephemeral("%lli", *val));
        return true;
    }
}

bool serialize_u64(Lpf_Dyn_Entry* entry, u64* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        isize index = 0;
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        return match_decimal_u64(readable.value, &index, val);
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("u"), format_ephemeral("%llu", *val));
        return true;
    }
}

bool serialize_vec4(Lpf_Dyn_Entry* entry, Vec4* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        
        isize index = 0;
        bool state = true;
        state = state && match_decimal_f32(readable.value, &index, &val->x);
        state = state && match_decimal_f32(readable.value, &index, &val->y);
        state = state && match_decimal_f32(readable.value, &index, &val->z);
        state = state && match_decimal_f32(readable.value, &index, &val->w);
        return state;
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("4f"), format_ephemeral("%f %f %f %f", val->x, val->y, val->z, val->w));
    }
}

bool serialize_vec3(Lpf_Dyn_Entry* entry, Vec3* val, Searialize_Options options)
{
    if(options.action == SERIALIZE_READ)
    {
        Lpf_Entry readable = lpf_entry_from_dyn_entry(*entry);
        
        isize index = 0;
        bool state = true;
        state = state && match_decimal_f32(readable.value, &index, &val->x);
        state = state && match_decimal_f32(readable.value, &index, &val->y);
        state = state && match_decimal_f32(readable.value, &index, &val->z);
        return state;
    }
    else
    {
        lpf_dyn_entry_set_value(entry, STRING("3f"), format_ephemeral("%f %f %f %f", val->x, val->y, val->z));
    }
}


bool serialize_texture(Lpf_Dyn_Entry* entry, Map* val, Resource_Info* info, Searialize_Options options)
{
    
    if(options.action == SERIALIZE_READ)
    {


    }
    else
    {
        #if 0
        Id id;

        String_Builder name;
        String_Builder path;
        Resource_Callstack callstack;
        i32 reference_count;
        u32 storage_index;
        u32 type_enum; //some enum value used for debugging

        i64 creation_etime;
        i64 death_etime;
        i64 modified_etime;
        i64 load_etime;
        i64 file_modified_etime; 

        Resource_Lifetime lifetime;
        Resource_Reload reload;
        #endif


        Lpf_Entry id = {LPF_KIND_ENTRY};

    }
}

