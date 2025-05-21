// #include <wine/windows/windows.h> hack to use lsp for windows in linux!
#include <debugapi.h>
#include <windows.h>

// #include <malloc.h>
// #include <stdint.h>
// #include <assert.h>
// #include <stdio.h>
// #include <string.h>
// #include <winuser.h>

int _fltused = 0; // magic !

void *__chkstk;

#define WGL_DRAW_TO_WINDOW_ARB                      0x2001
#define WGL_SUPPORT_OPENGL_ARB                      0x2010
#define WGL_DOUBLE_BUFFER_ARB                       0x2011
#define WGL_PIXEL_TYPE_ARB                          0x2013
#define WGL_TYPE_RGBA_ARB                           0x202B
#define WGL_COLOR_BITS_ARB                          0x2014
#define WGL_DEPTH_BITS_ARB                          0x2022
#define WGL_STENCIL_BITS_ARB                        0x2023
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB            0x20A9
#define WGL_SAMPLE_BUFFERS_ARB                      0x2041
#define WGL_SAMPLES_ARB                             0x2042
#define WGL_CONTEXT_MAJOR_VERSION_ARB               0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB               0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB                0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB            0x0001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB   0x0002
#define ERROR_INVALID_PROFILE_ARB                   0x2096
#define WGL_CONTEXT_DEBUG_BIT_ARB                   0x0001
#define WGL_CONTEXT_FLAGS_ARB                       0x2094
typedef HGLRC       (CreateContextAttribsARBType) (HDC hdc, void *hShareContext, int *attribList);
typedef BOOL        (ChoosePixelFormatARBType   ) (HDC hdc, int *attribIList, float *attribFList, DWORD maxFormats, int *formats, DWORD *numFormats);
typedef BOOL        (SwapIntervalEXTType        ) (int interval);
typedef const char* (GetExtensionsStringARBType ) (HDC);
CreateContextAttribsARBType *wglCreateContextAttribsARB;
ChoosePixelFormatARBType    *wglChoosePixelFormatARB;
SwapIntervalEXTType         *wglSwapIntervalEXT;
GetExtensionsStringARBType  *wglGetExtensionsStringARB;

#define ENABLE_VSYNC 1
#define GL_MAJOR     4
#define GL_MINOR     5

typedef struct Internal_System {
    // window
    HWND    hwnd;
    HDC     hdc;
    HGLRC   hglrc;
    int     viewport[2];

    //

} Internal_System;

static void fatal (const char *msg) {
    MessageBoxA(NULL, msg, "FATAL!", MB_ICONERROR);
    ExitProcess(~0);
}

static void load_wgl_functions (void) {
    WCHAR *class_name = L"STATIC";

    HWND dummy_hwnd = CreateWindowExW(
         0,
         class_name,
         class_name,
         WS_OVERLAPPED,
         CW_USEDEFAULT,
         CW_USEDEFAULT,
         CW_USEDEFAULT,
         CW_USEDEFAULT,
         NULL,
         NULL,
         NULL,
         NULL
    );
    assert(dummy_hwnd != NULL);

    HDC hdc = GetDC(dummy_hwnd);
    assert(hdc != NULL);

    PIXELFORMATDESCRIPTOR descriptor = {
        .nSize      = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion   = 1,
        .dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 24,
    };

    int format = ChoosePixelFormat(hdc, &descriptor);
    if (format == 0) {
        int err = GetLastError();
        char buf[1024];
        // sprintf(&buf[0], "Cannot choose OpenGL pixel format for dummy window! [%lu]\n", GetLastError());
        // fatal(buf);
        fatal("Cannot choose OpenGL pixel format for dummy window!\n");
    }

    if (DescribePixelFormat(hdc, format, sizeof(descriptor), &descriptor) == 0) {
        fatal("Failed to describe OpenGL pixel format");
    }

    if (!SetPixelFormat(hdc, format, &descriptor)) {
        int err = GetLastError();
        char buf[1024];
        // sprintf(&buf[0], "Cannot choose OpenGL pixel format for dummy window! [%lu]\n", GetLastError());
        // fatal(buf);
    }

    HGLRC glrc = wglCreateContext(hdc);
    if (glrc == NULL) {
        fatal("Failed to create OpenGL context for dummy window");
    }

    if (!wglMakeCurrent(hdc, glrc)) {
        int err = GetLastError();
        char buf[1024];
        // sprintf(&buf[0], "Failed to make current OpenGL context for dummy window [%lu]\n", GetLastError());
        // fatal(buf);
    }

    /////////////////////////////////////////
    /////////////////////////////////////////
    /////////////////////////////////////////

    // load the wgl functions

    wglGetExtensionsStringARB  = (GetExtensionsStringARBType*)  wglGetProcAddress("wglGetExtensionsStringARB");
    wglChoosePixelFormatARB    = (ChoosePixelFormatARBType*)    wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (CreateContextAttribsARBType*) wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT         = (SwapIntervalEXTType*)         wglGetProcAddress("wglSwapIntervalEXT");

    if ((wglChoosePixelFormatARB    == NULL) ||
        (wglGetExtensionsStringARB  == NULL) ||
        (wglCreateContextAttribsARB == NULL) ||
        (wglSwapIntervalEXT         == NULL))
    {
        fatal("OpenGL does not support required WGL extensions for Creating a context!");
    }

    /////////////////////////////////////////
    /////////////////////////////////////////
    /////////////////////////////////////////

    // cleanup
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(glrc);
    ReleaseDC(dummy_hwnd, hdc);
    DestroyWindow(dummy_hwnd);
}

static LRESULT window_proc (HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

static void w32_get_window_size (HWND hwnd, int *w, int *h) {
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    *w = rect.right - rect.left;
    *h = rect.bottom - rect.top;
}

static Internal_System * init_sys (void) {

    HMODULE kernelbase = LoadLibraryA("kernelbase.dll");
    if (kernelbase == NULL) {
        fatal("can't load kernelbase.dll for __chkstk");
    }
    __chkstk = GetProcAddress(kernelbase, "__chkstk");

    load_wgl_functions();

    Internal_System *sys = calloc(1, sizeof(*sys));

    WCHAR *wname = L"TODO_LOAD_FROM_FILE_OR_DEF";
    int height = 720;
    int width = height * 16 / 9;

    HMODULE hinstance = GetModuleHandleW(NULL);
    assert(hinstance != NULL && "Failed to get module handle");

    // register window class to have custom WindowProc callback
    WNDCLASSEXW wc = {
        .cbSize        = sizeof(WNDCLASSEXW),
        .lpfnWndProc   = window_proc,
        .hInstance     = (HANDLE) hinstance,
        .hIcon         = LoadIconA(NULL, IDI_APPLICATION),
        .hCursor       = LoadCursorA(NULL, IDC_ARROW),
        .lpszClassName = wname
    };

    ATOM atom = RegisterClassExW(&wc);
    assert(atom != 0 && "Failed to register window class");

    // set window style/properties
    int exstyle = WS_EX_APPWINDOW;
    int style = WS_OVERLAPPEDWINDOW;

    // fix window size
    style &= ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT rect = {0, 0, width, height};
    AdjustWindowRectEx(&rect, style, FALSE, exstyle);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    // borderless
    style &= ~WS_CAPTION;
    style &= ~WS_BORDER;
    // style |= WS_POPUPWINDOW // this gives a thin line around borders
    style |= WS_POPUP;

    // screen size
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    int x = 0, y = 0;
    // x = (screen_w - width) / 2
    // y = (screen_h - height) / 2

    // always on top
    // exstyle |= WS_EX_TOPMOST

    // create window
    sys->hwnd = CreateWindowExW(
        exstyle,
        wname,
        wname,
        style,
        x,
        y,
        width,
        height,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );

    assert(sys->hwnd != NULL && "Failed to create window");

    sys->hdc = GetDC(sys->hwnd);
    if (sys->hdc == NULL) {
        int err = GetLastError();
        char buf[1024];
        // sprintf(&buf[0], "Failed to window device context [%lu].\n", GetLastError());
        // fatal(buf);
    }

    {     // set pixel format for OpenGL context
        int attrib[] = {

            WGL_DRAW_TO_WINDOW_ARB, 1, // GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, 1, // GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  1, // GL_TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     24,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,

            // uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_sRGB extension
            // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, 1, // GL_TRUE,

            // uncomment for multisampled framebuffer, from WGL_ARB_multisample extension
            // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
            WGL_SAMPLE_BUFFERS_ARB, 1,
            WGL_SAMPLES_ARB,        4, // 4x MSAA

            0,
        };

        INT format;
        DWORD formats;
        if (!wglChoosePixelFormatARB(sys->hdc, attrib, NULL, 1, &format, &formats) || formats == 0) {
            fatal("OpenGL does not support required pixel format!");
        }

        PIXELFORMATDESCRIPTOR desc = {
            .nSize = sizeof(PIXELFORMATDESCRIPTOR),
        };

        if (DescribePixelFormat(sys->hdc, format, sizeof(desc), &desc) == 0) {
            fatal("Failed to describe OpenGL pixel format");
        }

        if (!SetPixelFormat(sys->hdc, format, &desc)) {
            fatal("Cannot set OpenGL selected pixel format!");
        }
    } //

    {     // create modern OpenGL context

        int attrib[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, GL_MAJOR,
            WGL_CONTEXT_MINOR_VERSION_ARB, GL_MINOR,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        #if DEBUG_BUILD
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
        #endif
            0, // last must be zero
        };

        sys->hglrc = wglCreateContextAttribsARB(sys->hdc, NULL, attrib);

        if (sys->hglrc == NULL) {
            fatal("Cannot create modern OpenGL context! OpenGL version 4.5 not supported?");
        }

        if (!wglMakeCurrent(sys->hdc, sys->hglrc)) {
            fatal("Failed to make current OpenGL context");
        }

        // load OpenGL functions
        // gl.load_up_to(GL_MAJOR, GL_MINOR, gl_set_proc_address)

        // gl.Enable(gl.MULTISAMPLE)

        // when ODIN_DEBUG {
            // enable debug callback
            // gl.DebugMessageCallback(gl_debug_callback, NULL)
            // gl.Enable(gl.DEBUG_OUTPUT_SYNCHRONOUS)
        // }
    }

    // set to FALSE to disable vsync
    wglSwapIntervalEXT(ENABLE_VSYNC ? 1 : 0);

    // show window
    ShowWindow(sys->hwnd, SW_SHOWDEFAULT);

    w32_get_window_size(sys->hwnd, &sys->viewport[0], &sys->viewport[1]);

    ///////////////////////////////
    ///////////////////////////////

    // print init info

    // gl_version = gl.GetString(gl.VERSION)
    // gl_vendor = gl.GetString(gl.VENDOR)
    // gl_render = gl.GetString(gl.RENDERER)

    // log.infof("*** Started %v %v", ODIN_OS, ODIN_ARCH_STRING)
    // log.infof("*** OpenGL %v.%v (%v)", GL_MAJOR, GL_MINOR, gl_version)
    // log.infof("*** GL vendor: %v", gl_vendor)
    // log.infof("*** GL renderer: %v", gl_render)
    // log.infof("*** Canvas Size: %v", window.size)

    ///////////////////////////////
    ///////////////////////////////

    return sys;
}

/* ********************************************************************************************
 * MAIN Main main Main wMain eMain KaMain FoMain T-Main Mr-Ain
 * *** */

// mainCRTStartup
// WinMainCRTStartup
// int mainCRTStartup (LPVOID var1, DWORD var2, LPVOID var3) {
int WinMain (HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    Internal_System *sys = init_sys();
    platform_entry(sys);
    return 0;
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

void quit_sys (rawptr sys, b32 save_on_exit) {
    if (save_on_exit) {
        panic_todo();
    }
    ExitProcess(0);
}

void pull_system_events (rawptr _sys) {
    assert(_sys != NULL);
    Internal_System *sys = _sys;

    MSG msg; while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
        case WM_QUIT:
            ExitProcess(0);
            break;

        case WM_SIZING:
            w32_get_window_size(sys->hwnd, &sys->viewport[0], &sys->viewport[1]);
            break;

        // keyboard
        case WM_KEYDOWN:
            if (msg.wParam == VK_ESCAPE) {
                quit_sys(sys, 0);
            }
            break;
        case WM_KEYUP:
            break;

        // mouse
        case WM_LBUTTONDOWN:
            break;
        case WM_LBUTTONUP:
            break;

        case WM_RBUTTONDOWN:
            break;
        case WM_RBUTTONUP:
            break;

        case WM_MOUSEMOVE:
            // x = cast(i32)(msg.lParam)       & 0xFFFF,
            // y = cast(i32)(msg.lParam >> 16) & 0xFFFF,
            break;

        default:
            // ignore
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    } // events loop


    // TODO: ADD_TIMER
}

void swap_buffers (rawptr _sys) {
    assert(_sys != NULL);
    Internal_System *sys = _sys;

    // window is minimized, cannot vsync - instead sleep a bit
    if ((sys->viewport[0] == 0) || (sys->viewport[0] == 0)) {
        #if ENABLE_VSYNC
            Sleep(10);
        #endif
        return;
    }

    //
    if (!wglMakeCurrent(sys->hdc, sys->hglrc)) {
        fatal("Failed to make current OpenGL context");
    }

    if (!SwapBuffers(sys->hdc)) {
        fatal("Failed to swap OpenGL buffers!");
    }
}

void make_render_current (rawptr _sys) {
    assert(_sys != NULL);
    Internal_System *sys = _sys;
    if (!wglMakeCurrent(sys->hdc, sys->hglrc)) {
        fatal("Failed to make current OpenGL context");
    }
}

void debug_print (const char *msg) {
    if (IsDebuggerPresent()) {
        OutputDebugStringA(msg);
    }
}

void print (const char *msg) {
    debug_print(msg);
}

void wprint (const char *msg) {
    debug_print(msg);
}

void eprint (const char *msg) {
    debug_print(msg);
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

u64 time_now(void) {
	panic_todo();
}

u64 time_since(u64 old_snap_shot) {
	panic_todo();
}

// returns delta and updates old value (usefull for loops)
u64 time_tick(u64 *old_snap_shot) {
	panic_todo();
}

/* ********************************************************************************************
 * Rendering Backend
 * *** */

Backend_Rendering_API get_render_api(rawptr sys) {
    (void) sys;
    return BACKEND_OPENGL;
}

Backend_Rendering_Interface get_render_interface(rawptr sys) {
    (void) sys;
    return BACKEND_WGL;
}

// HWND  | EGLDisplay
rawptr get_render_display (rawptr _sys) {
    assert(_sys != NULL);
    Internal_System *sys = _sys;
    return sys->hwnd;
}

// HGLRC | EGLContext
rawptr get_render_context (rawptr _sys) {
    assert(_sys != NULL);
    Internal_System *sys = _sys;
    return sys->hglrc;
}

// HDC | EGLSurface
rawptr get_render_surface (rawptr _sys) {
    assert(_sys != NULL);
    Internal_System *sys = _sys;
    return sys->hdc;
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

