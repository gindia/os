package game

import os "../os"
import gl "vendor:OpenGL"
import "base:runtime"
import "core:fmt"
import "core:log"

@(export, link_name="game_entry")
game_entry :: proc "c" (sys: rawptr) {
    context = runtime.default_context()
    context.logger = os.logger()

    // runtime.debug_trap()

    gl.load_up_to(4, 5, os.gl_set_proc_address)

    for {
        os.pull_system_events(sys)

        gl.ClearColor(0.1, 0.8, 0.1, 0.0)
        gl.Clear(gl.COLOR_BUFFER_BIT)

        os.swap_buffers(sys)
    }
}
