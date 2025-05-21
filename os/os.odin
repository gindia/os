package unified_os

// import "core:c"
// _ :: c

Backend_Rendering_Interface :: enum int {
	WGL = 0, // windows
	EGL,     // android & linux
}

Backend_Rendering_API :: enum int {
	OPENGL = 0, // windows
	GLES,       // android & linux
}

Controller_Action :: enum int {
	None = 0,
	Click_Left,  // A | left arrow
	Click_Right, // D | right arrow
	Pinch_In,    // W | up arrow
	Pinch_Out,   // S | down arrow
	Swipe_Left,  // E | Z
	Swipe_Right, // Q | X
}

when OS == .Windows {
    when ODIN_ARCH == .amd64 {
        // msvcrt libcmt
        @(extra_linker_flags="/NODEFAULTLIB:msvcrt /ENTRY:mainCRTStartup") //static lib
        // @(extra_linker_flags="/ENTRY:mainCRTStartup")
        foreign import lib {
            // "./lib/platform-windows-x86_64.obj",
            "system:kernel32.lib",
            "system:user32.lib",
            "system:gdi32.lib",
            "system:opengl32.lib",

			// "system:libcmt.lib",
            // "system:msvcrt.lib",

	    "system:libucrtd.lib",
	    "system:libvcruntimed.lib",
	    "system:libcmtd.lib",
	    "system:libcpmt.lib",
        }
    } else {
	#panic("Unknown/Unsupported ARCH")
    }
} else when OS_ANDROID {
    when ODIN_ARCH == .arm64 {
	foreign import lib {
	    // "./lib/platform-android31-aarch64.o",
	    "system:android",
	    "system:dl",
	    "system:log",
	    "system:EGL",
	    "system:GLESv2",
	}
    } else when ODIN_ARCH == .amd64 {
	foreign import lib {
	    // "./lib/platform-android31-x86_64.o",
	    "system:android",
	    "system:dl",
	    "system:log",
	    "system:EGL",
	    "system:GLESv2",
	}
    } else {
	#panic("Unknown/Unsupported ARCH")
    }
} else {
    #panic("Unknown/Unsupported OS")
}

@(default_calling_convention="c", link_prefix="")
foreign lib {
	/* ********************************************************************************************
	* Game Entry
	*
	*    NOTE(gindia): diffrent platforms have a diffrent entry points. Hence
	*                  using game_entry as a common access point between platfoms.
	* *** */
	// game_entry :: proc(sys: rawptr) --- // define this in game code.

	/* ********************************************************************************************
	* System
	* *** */
	init_game_state     :: proc(sys: rawptr, game: rawptr, game_size: u64) --- // sets the internal ptrs to the game stae
	save_game_state     :: proc(sys: rawptr, name: cstring) ---                // save game state to   desk.
	load_game_state     :: proc(sys: rawptr, name: cstring) ---                // load game state from desk.
	quit_sys            :: proc(sys: rawptr, save_on_exit: b32) ---
	pull_system_events  :: proc(sys: rawptr) ---
	swap_buffers        :: proc(sys: rawptr) ---
	make_render_current :: proc(sys: rawptr) ---
	debug_print         :: proc(msg: cstring) ---
	print		    :: proc(msg: cstring) ---
	wprint		    :: proc(msg: cstring) ---
	eprint		    :: proc(msg: cstring) ---

	/* ********************************************************************************************
	* Time
	* *** */
	get_elapsed_time   :: proc(sys: rawptr) -> u64 ---
	get_elapsed_frames :: proc(sys: rawptr) -> u64 ---
	time_now           :: proc() -> u64 ---
	time_since         :: proc(old_snap_shot: u64) -> u64 ---
	time_tick          :: proc(old_snap_shot: ^u64) -> u64 --- // returns delta and updates old value (usefull for loops)

	/* ********************************************************************************************
	* Rendering Backend
	* *** */
	get_render_api       :: proc(sys: rawptr) -> Backend_Rendering_API ---
	get_render_interface :: proc(sys: rawptr) -> Backend_Rendering_Interface ---
	get_render_display   :: proc(sys: rawptr) -> rawptr ---                      // HWND  | EGLDisplay
	get_render_context   :: proc(sys: rawptr) -> rawptr ---                      // HGLRC | EGLContext
	get_render_surface   :: proc(sys: rawptr) -> rawptr ---                      // HDC   | EGLSurface

	/* ********************************************************************************************
	* Assets (Files)
	* *** */
	get_asset_size :: proc(sys: rawptr, name: cstring) -> u64 ---

	// make sure that the buffer can fit the data using get_asset_size()
	get_asset_data :: proc(sys: rawptr, name: cstring, buffer: rawptr, buffer_cap: u64) -> b32 ---
	set_asset      :: proc(sys: rawptr, name: cstring, data: rawptr, data_size: u64, over_write_if_exists: b32) -> b32 ---

	/* ********************************************************************************************
	* Controller
	* *** */
	get_controller_action :: proc(sys: rawptr) -> Controller_Action ---
}
