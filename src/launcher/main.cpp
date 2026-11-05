#include <windows.h>
#include "gui.h"

extern void mute_ti_etw();

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    mute_ti_etw();
    gui::run();
    return 0;
}