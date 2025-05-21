package unified_os

import egl "vendor:egl"
import w32 "core:sys/windows"
import "base:runtime"
import fmt "core:fmt"

OS_ANDROID     : bool   : #config(OS_ANDROID,     false)
ANDROID_API    : string : #config(ANDROID_API,    "31")
ANDROID_APP_ID : string : #config(ANDROID_APP_ID, "")

OS_Type :: enum {
    Windows,
    Android,
}

OS : OS_Type : .Android   when OS_ANDROID	   else
               .Windows   when ODIN_OS == .Windows else #panic("Unknown/Unsupported OS")

gl_set_proc_address :: proc(p: rawptr, name: cstring) {
    when OS == .Windows {
        w32.gl_set_proc_address(p, name)
    } else when OS == .Android {
        egl.gl_set_proc_address(p, name)
    }
}

logger :: proc () -> runtime.Logger {
    return {
        procedure = proc(data: rawptr, level: runtime.Logger_Level, text: string, options: runtime.Logger_Options, location := #caller_location) {
            switch level {
            case .Debug:   debug_print(fmt.ctprintf("[debg] %s", text))
            case .Info:    print(fmt.ctprintf("[info] %s", text))
            case .Warning: wprint(fmt.ctprintf("[warn] %s", text))
            case .Error:   eprint(fmt.ctprintf("[erro] %s", text))
            case .Fatal:   panic(fmt.tprintf("[fatal] %s", text), loc = location)
            }
        },
        data = nil,
        lowest_level = .Info,
        options = {},
    }
}
