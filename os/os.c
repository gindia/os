#include "os.h"

static void platform_entry (rawptr internal);

#if   defined (OS_WINDOWS)
    #include "impl_windows/sys.c"
#elif defined (OS_ANDROID)
    #include "impl_android/sys.c"
#else
    #error "Missing definition of OS_{WINDOWS|ANDROID}"
#endif

static void platform_entry (rawptr internal) {
    game_entry(internal);
}
