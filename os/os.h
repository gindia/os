#pragma once

// #define crash() do { *(int*) 0 = 0; } while(0)
#define crash() do { __builtin_trap(); } while(0)
#define assert(cond) if (!(cond)) { crash(); }
#define panic_todo() assert(0 && "TODO!")

typedef enum Backend_Rendering_Interface {
    BACKEND_WGL = 0, // windows
    BACKEND_EGL,     // android & linux
} Backend_Rendering_Interface;

typedef enum Backend_Rendering_API {
    BACKEND_OPENGL = 0, // windows
    BACKEND_GLES,       // android & linux
} Backend_Rendering_API;

typedef enum Controller_Action {
    Controlller_Action_None = 0,
    Controlller_Action_Click_Left,  // A | left arrow
    Controlller_Action_Click_Right, // D | right arrow
    Controlller_Action_Pinch_In,    // W | up arrow
    Controlller_Action_Pinch_Out,   // S | down arrow
    Controlller_Action_Swipe_Left,  // E | Z
    Controlller_Action_Swipe_Right, // Q | X
} Controller_Action;

typedef void * rawptr;
typedef long long unsigned int u64;
typedef int b32;

/* ********************************************************************************************
 * Game Entry
 *
 *    NOTE(gindia): diffrent platforms have a diffrent entry points. Hence
 *                  using game_entry as a common access point between platfoms.
 * *** */
extern void game_entry (rawptr sys); // define this in game code.

/* ********************************************************************************************
 * System
 * *** */

void   init_game_state     (rawptr sys, rawptr game, u64 game_size); // sets the internal ptrs to the game stae
void   save_game_state     (rawptr sys, const char *name); // save game state to   desk.
void   load_game_state     (rawptr sys, const char *name); // load game state from desk.
void   quit_sys            (rawptr sys, b32 save_on_exit);
void   pull_system_events  (rawptr sys);
void   swap_buffers        (rawptr sys);
void   make_render_current (rawptr sys);
void   debug_print         (const char *msg);
void   print               (const char *msg);
void   wprint              (const char *msg);
void   eprint              (const char *msg);

/* ********************************************************************************************
 * Time
 * *** */

u64 get_elapsed_time    (rawptr sys);
u64 get_elapsed_frames  (rawptr sys);
u64 time_now            (void);
u64 time_since          (u64 old_snap_shot);
u64 time_tick           (u64 *old_snap_shot); // returns delta and updates old value (usefull for loops)

/* ********************************************************************************************
 * Rendering Backend
 * *** */

Backend_Rendering_API       get_render_api       (rawptr sys);
Backend_Rendering_Interface get_render_interface (rawptr sys);
rawptr                      get_render_display   (rawptr sys); // HWND  | EGLDisplay
rawptr                      get_render_context   (rawptr sys); // HGLRC | EGLContext
rawptr                      get_render_surface   (rawptr sys); // HDC   | EGLSurface

/* ********************************************************************************************
 * Assets (Files)
 * *** */

u64 get_asset_size (rawptr sys, const char *name);
// make sure that the buffer can fit the data using get_asset_size()
b32 get_asset_data (rawptr sys, const char *name, rawptr buffer, u64 buffer_cap);
b32 set_asset      (rawptr sys, const char *name, rawptr data, u64 data_size, b32 over_write_if_exists);

/* ********************************************************************************************
 * Controller
 * *** */

Controller_Action get_controller_action (rawptr sys);
