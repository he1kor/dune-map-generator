#pragma once

#include <cstdlib>

namespace syssound {

#ifdef _WIN32
    #include <windows.h>
    
    inline void playError() {
        MessageBeep(MB_ICONERROR);
    }
    
    inline void playWarning() {
        MessageBeep(MB_ICONWARNING);
    }
    
    inline void playInfo() {
        MessageBeep(MB_ICONINFORMATION);
    }
#endif

}