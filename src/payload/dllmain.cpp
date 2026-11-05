#include <windows.h>

namespace scan {
    bool discover_offsets();
    void* find_script_context();
}
namespace dumper {
    void rip_all(void* sctx);
}

static DWORD WINAPI worker(LPVOID) {
    if (!scan::discover_offsets()) return 1;
    void* sctx = scan::find_script_context();
    if (!sctx) return 2;
    dumper::rip_all(sctx);
    return 0;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}