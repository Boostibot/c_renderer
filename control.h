#include "lib/defines.h"
#include "glfw/glfw3.h"

#define CONTROL_MAPPING_SETS 3
typedef enum Control_Name
{
    CONTROL_NONE = 0,

    /* mouse on screen when mouse is visible */
    CONTROL_MOUSE_X, 
    CONTROL_MOUSE_Y,

    /* scrolling while mouse is visible*/
    CONTROL_SCROLL, 

    /* looking around in a 3D world*/
    CONTROL_LOOK_X, 
    CONTROL_LOOK_Y,
    CONTROL_ZOOM,

    CONTROL_MOVE_FORWARD,
    CONTROL_MOVE_BACKWARD,
    CONTROL_MOVE_RIGHT,
    CONTROL_MOVE_LEFT,
    CONTROL_MOVE_UP,
    CONTROL_MOVE_DOWN,

    CONTROL_JUMP,
    CONTROL_INTERACT,
    CONTROL_ESCAPE,
    CONTROL_PAUSE,
    CONTROL_SPRINT,
    
    CONTROL_REFRESH_SHADERS,
    CONTROL_REFRESH_ART,
    CONTROL_REFRESH_CODE,
    CONTROL_REFRESH_ALL,

    CONTROL_DEBUG_1,
    CONTROL_DEBUG_2,
    CONTROL_DEBUG_3,
    CONTROL_DEBUG_4,
    CONTROL_DEBUG_5,
    CONTROL_DEBUG_6,
    CONTROL_DEBUG_7,
    CONTROL_DEBUG_8,
    CONTROL_DEBUG_9,
    CONTROL_DEBUG_10,

    CONTROL_COUNT,
} Control_Name;

//just for consistency here
#define GLFW_MOUSE_X 0
#define GLFW_MOUSE_Y 1
#define GLFW_MOUSE_SCROLL 2
#define GLFW_MOUSE_LAST GLFW_MOUSE_SCROLL

//Maps physical inputs to abstract game actions
//each entry can be assigned to one action.
//values are interpreted as Control_Name
//A key can be assigned to multiple actions simultaneously
// by duplicating this structure
typedef struct Control_Mapping
{
    u8 mouse[GLFW_MOUSE_LAST + 1];
    u8 mouse_buttons[GLFW_MOUSE_BUTTON_LAST + 1];
    u8 keys[GLFW_KEY_LAST + 1];
    //@TODO: Joystick and gamepads later....
} Control_Mapping;

typedef enum Input_Type
{
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_MOUSE_BUTTON,
    INPUT_TYPE_KEY
} Input_Type;

//An abstarct set of game actions to be used by the game code
//we represent all values as amountand the number of interactions
//this allows us to rebind pretty much everything to everything including 
//keys to control camera and mouse to control movement.
//This is useful for example because controllers use smooth controls for movement while keyboard does not
//The values range from -1 to 1 where 0 indicates off and nonzero indicates on. 
typedef struct Controls
{
    f32 values[CONTROL_COUNT];
    u8 interactions[CONTROL_COUNT];
} Controls;

bool control_was_pressed(Controls* controls, Control_Name control)
{
    u8 interactions = controls->interactions[control];
    f32 value = controls->values[control];

    //was pressed and released at least once
    if(interactions > 2)
        return true;

    //was interacted with once to pressed state
    if(interactions == 1 && value != 0.0f)
        return true;

    return false;
}

bool control_was_released(Controls* controls, Control_Name control)
{
    u8 interactions = controls->interactions[control];
    f32 value = controls->values[control];

    //was pressed and released at least once
    if(interactions > 2)
        return true;

    //was interacted with once to released state
    if(interactions == 1 && value == 0.0f)
        return true;

    return false;
}

bool control_is_down(Controls* controls, Control_Name control)
{
    f32 value = controls->values[control];
    return value != 0;
}

u8* control_mapping_get_entry(Control_Mapping* mapping, Input_Type type, isize index)
{
    switch(type)
    {
        case INPUT_TYPE_MOUSE:        
            CHECK_BOUNDS(index, GLFW_MOUSE_LAST + 1); 
            return &mapping->mouse[index];
        case INPUT_TYPE_MOUSE_BUTTON: 
            CHECK_BOUNDS(index, GLFW_MOUSE_BUTTON_LAST + 1); 
            return &mapping->mouse_buttons[index];
        case INPUT_TYPE_KEY:          
            CHECK_BOUNDS(index, GLFW_KEY_LAST + 1); 
            return &mapping->keys[index];
        default: ASSERT(false); return NULL;
    }
}

void control_mapping_add(Control_Mapping mappings[CONTROL_MAPPING_SETS], Control_Name control, Input_Type type, isize index)
{
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &mappings[i];
        u8* control_slot = control_mapping_get_entry(mapping, type, index);
        if(*control_slot == CONTROL_NONE)
        {
            *control_slot = control;
            break;
        }
    }
}

void mapping_make_default(Control_Mapping mappings[CONTROL_MAPPING_SETS])
{
    memset(mappings, 0, sizeof(Control_Mapping) * CONTROL_MAPPING_SETS);

    const Input_Type MOUSE = INPUT_TYPE_MOUSE;
    const Input_Type KEY = INPUT_TYPE_KEY;
    const Input_Type MOUSE_BUTTON = INPUT_TYPE_MOUSE_BUTTON;

    (void) MOUSE_BUTTON;

    control_mapping_add(mappings, CONTROL_MOUSE_X,      MOUSE, GLFW_MOUSE_X);
    control_mapping_add(mappings, CONTROL_MOUSE_Y,      MOUSE, GLFW_MOUSE_Y);
    control_mapping_add(mappings, CONTROL_LOOK_X,       MOUSE, GLFW_MOUSE_X);
    control_mapping_add(mappings, CONTROL_LOOK_Y,       MOUSE, GLFW_MOUSE_Y);
    control_mapping_add(mappings, CONTROL_SCROLL,       MOUSE, GLFW_MOUSE_SCROLL);
    control_mapping_add(mappings, CONTROL_ZOOM,         MOUSE, GLFW_MOUSE_SCROLL);
    
    control_mapping_add(mappings, CONTROL_MOVE_FORWARD, KEY, GLFW_KEY_W);
    control_mapping_add(mappings, CONTROL_MOVE_BACKWARD,KEY, GLFW_KEY_S);
    control_mapping_add(mappings, CONTROL_MOVE_RIGHT,   KEY, GLFW_KEY_D);
    control_mapping_add(mappings, CONTROL_MOVE_LEFT,    KEY, GLFW_KEY_A);
    control_mapping_add(mappings, CONTROL_MOVE_UP,      KEY, GLFW_KEY_SPACE);
    control_mapping_add(mappings, CONTROL_MOVE_DOWN,    KEY, GLFW_KEY_LEFT_SHIFT);
    
    control_mapping_add(mappings, CONTROL_MOVE_FORWARD, KEY, GLFW_KEY_UP);
    control_mapping_add(mappings, CONTROL_MOVE_FORWARD, KEY, GLFW_KEY_DOWN);
    control_mapping_add(mappings, CONTROL_MOVE_RIGHT,   KEY, GLFW_KEY_RIGHT);
    control_mapping_add(mappings, CONTROL_MOVE_RIGHT,   KEY, GLFW_KEY_LEFT);
    
    control_mapping_add(mappings, CONTROL_JUMP,         KEY, GLFW_KEY_SPACE);
    control_mapping_add(mappings, CONTROL_INTERACT,     KEY, GLFW_KEY_E);
    control_mapping_add(mappings, CONTROL_ESCAPE,       KEY, GLFW_KEY_ESCAPE);
    control_mapping_add(mappings, CONTROL_PAUSE,        KEY, GLFW_KEY_ESCAPE);
    control_mapping_add(mappings, CONTROL_SPRINT,       KEY, GLFW_KEY_LEFT_ALT);
    
    control_mapping_add(mappings, CONTROL_REFRESH_ALL,  KEY, GLFW_KEY_R);
    control_mapping_add(mappings, CONTROL_DEBUG_1,      KEY, GLFW_KEY_F1);
    control_mapping_add(mappings, CONTROL_DEBUG_2,      KEY, GLFW_KEY_F2);
    control_mapping_add(mappings, CONTROL_DEBUG_3,      KEY, GLFW_KEY_F3);
    control_mapping_add(mappings, CONTROL_DEBUG_4,      KEY, GLFW_KEY_F4);
    control_mapping_add(mappings, CONTROL_DEBUG_5,      KEY, GLFW_KEY_F5);
    control_mapping_add(mappings, CONTROL_DEBUG_6,      KEY, GLFW_KEY_F6);
    control_mapping_add(mappings, CONTROL_DEBUG_7,      KEY, GLFW_KEY_F7);
    control_mapping_add(mappings, CONTROL_DEBUG_8,      KEY, GLFW_KEY_F8);
    control_mapping_add(mappings, CONTROL_DEBUG_9,      KEY, GLFW_KEY_F9);
    control_mapping_add(mappings, CONTROL_DEBUG_10,     KEY, GLFW_KEY_F10);
}