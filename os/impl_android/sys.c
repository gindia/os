#include <jni.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <stdint.h>

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)

#define LOG_TAG "gg-native-activity"

#ifdef ANDROID_APP_ID
    #define APP_ID QUOTE(ANDROID_APP_ID)
#else
    #error "ANDROID_APP_ID is not defined e.g. `com.company.app_name`"
#endif

#include "native_app_glue/android_native_app_glue.h"
#include "native_app_glue/android_native_app_glue.c"

// #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))

typedef struct Internal_System {
    struct android_app *app;

    ASensorManager    *sensor_manager;
    const ASensor     *accelerometer_sensor;
    ASensorEventQueue *sensor_event_queue;

    b32        animating;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int        viewport[2];

} Internal_System;

// android callbacks
static void     android_on_app_cmd      (struct android_app *app, int32_t cmd);
static int32_t  android_on_input_event  (struct android_app* app, AInputEvent* event);

static Internal_System * init_sys (struct android_app* app) {
    Internal_System *sys = calloc(1, sizeof(*sys));

    app->userData = sys;
    app->onAppCmd = android_on_app_cmd;
    app->onInputEvent = android_on_input_event;

    sys->app = app;

    sys->sensor_manager = ASensorManager_getInstanceForPackage(APP_ID);
    sys->accelerometer_sensor = ASensorManager_getDefaultSensor(sys->sensor_manager, ASENSOR_TYPE_ACCELEROMETER);
    sys->sensor_event_queue = ASensorManager_createEventQueue(sys->sensor_manager, app->looper, LOOPER_ID_USER, NULL, NULL);

    LOGI("%s\n", "Started Android platform-layer");
    return sys;
}

/**
 * Initialize an EGL context for the current display.
 */
static int init_display (Internal_System *sys) {
    // initialize OpenGL ES and EGL

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE,    8,
            EGL_GREEN_SIZE,   8,
            EGL_RED_SIZE,     8,
            EGL_NONE
    };
    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
     * sample, we have a very simplified selection process, where we pick
     * the first EGLConfig that matches our criteria */
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(sys->app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType) sys->app->window, NULL);
    context = eglCreateContext(display, config, NULL, NULL);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    sys->display = display;
    sys->context = context;
    sys->surface = surface;
    sys->viewport[0] = w;
    sys->viewport[1] = h;
    // internal->state.angle = 0;

    // Initialize GL state.
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
    // glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    return 0;
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void term_display (Internal_System *sys) {
    if (sys->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(sys->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (sys->context != EGL_NO_CONTEXT) {
            eglDestroyContext(sys->display, sys->context);
        }
        if (sys->surface != EGL_NO_SURFACE) {
            eglDestroySurface(sys->display, sys->surface);
        }
        eglTerminate(sys->display);
    }
    sys->animating = 0;
    sys->display = EGL_NO_DISPLAY;
    sys->context = EGL_NO_CONTEXT;
    sys->surface = EGL_NO_SURFACE;
}


/**
 * Process the next main command.
 */
static void android_on_app_cmd (struct android_app *app, int32_t cmd) {
    Internal_System *sys = app->userData;
    switch (cmd) {

        // save state
        case APP_CMD_SAVE_STATE:
            // The system has asked us to save our current state.  Do so.
            // sys->app->savedState = calloc(1, sys->state_size);
            // memcpy(internal->app->savedState, internal->state, internal->state_size);
            // internal->app->savedStateSize = internal->state_size;
            break;

        // init window
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (sys->app->window != NULL) {
                init_display(sys);
            }
            break;

        // deinit window
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            term_display(sys);
            break;

        // ++ gain focus ++
        case APP_CMD_GAINED_FOCUS:
            // When our app gains focus, we start monitoring the accelerometer.
            if (sys->accelerometer_sensor != NULL) {
                ASensorEventQueue_enableSensor(sys->sensor_event_queue, sys->accelerometer_sensor);
                // We'd like to get 60 events per second (in us).
                ASensorEventQueue_setEventRate(sys->sensor_event_queue, sys->accelerometer_sensor, (1000L/60)*1000);

                // gain animating
                sys->animating = 1;
            }
            break;

        // -- lost focus --
        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
            if (sys->accelerometer_sensor != NULL) {
                ASensorEventQueue_disableSensor(sys->sensor_event_queue, sys->accelerometer_sensor);
            }
            // Also stop animating.
            sys->animating = 0;
            break;
    }
}


/**
 * Process the next input event.
 */
static int32_t android_on_input_event (struct android_app* app, AInputEvent* event) {
    Internal_System *sys = app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        // internal->animating = 1;
        // internal->input.x = AMotionEvent_getX(event, 0);
        // internal->input.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/* ********************************************************************************************
 * MAIN Main main Main wMain eMain KaMain FoMain T-Main Mr-Ain
 * *** */
extern void android_main (struct android_app* state) {
    Internal_System *sys = init_sys(state);
    platform_entry(sys);
}

/* ********************************************************************************************
 * System
 * *** */

// sets the internal ptrs to the game stae
void init_game_state (rawptr sys, rawptr game, u64 game_size) {
    panic_todo();
}

// save game state to   desk.
void save_game_state (rawptr sys, const char *name) {
    panic_todo();
}

// load game state from desk.
void load_game_state (rawptr sys, const char *name) {
    panic_todo();
}

//
void quit_sys (rawptr sys, b32 save_on_exit) {
    panic_todo();
}

void pull_system_events (rawptr _sys) {
    Internal_System *sys = _sys;

    // Read all pending events.
    struct android_poll_source* source;
    int result = ALooper_pollOnce(sys->animating ? 0 : -1, NULL, NULL, (void**)&source);
    if (result == ALOOPER_POLL_ERROR) {
        assert(0 && "ALooper_pollOnce returned error.");
    }

    // Process this event.
    if (source != NULL) {
        source->process(sys->app, source);
    }
}

void swap_buffers (rawptr _sys) {
    Internal_System *sys = _sys;
    if (sys->display != EGL_NO_DISPLAY && sys->context != EGL_NO_SURFACE) {
        eglSwapBuffers(sys->display, sys->surface);
    }
}

void make_render_current (rawptr _sys) {
    Internal_System *sys = _sys;
    if (eglMakeCurrent(sys->display, sys->surface, sys->surface, sys->context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
    }
}

void debug_print (const char *msg) {
    (void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s\n", msg);
}

void print (const char *msg) {
    (void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s\n", msg);
}

void wprint (const char *msg) {
    (void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "%s\n", msg);
}

void eprint (const char *msg) {
    (void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s\n", msg);
}

/* ********************************************************************************************
 * Time
 * *** */

u64 get_elapsed_time (rawptr sys) {
    panic_todo();
}

u64 get_elapsed_frames (rawptr sys) {
    panic_todo();
}

u64 time_now (void) {
    panic_todo();
}

u64 time_since (u64 old_snap_shot) {
    panic_todo();
}

u64 time_tick (u64 *old_snap_shot) {
    panic_todo();
}


/* ********************************************************************************************
 * Rendering Backend
 * *** */

Backend_Rendering_API get_render_api (rawptr sys) {
    (void) sys;
    return BACKEND_GLES;
}

Backend_Rendering_Interface get_render_interface (rawptr sys) {
    (void) sys;
    return BACKEND_EGL;
}

rawptr get_render_display (rawptr _sys) {
    Internal_System *sys = _sys;
    return sys->display;
}

rawptr get_render_context (rawptr _sys) {
    Internal_System *sys = _sys;
    return sys->context;
}

rawptr get_render_surface (rawptr _sys) {
    Internal_System *sys = _sys;
    return sys->surface;
}


/* ********************************************************************************************
 * Assets (Files)
 * *** */

u64 get_asset_size (rawptr sys, const char *name) {
    panic_todo();
}

// make sure that the buffer can fit the data using get_asset_size()
b32 get_asset_data (rawptr sys, const char *name, rawptr buffer, u64 buffer_cap) {
    panic_todo();
}

b32 set_asset (rawptr sys, const char *name, rawptr data, u64 data_size, b32 over_write_if_exists) {
    panic_todo();
}


/* ********************************************************************************************
 * Controller
 * *** */

Controller_Action get_controller_action (rawptr sys) {
    panic_todo();
}
