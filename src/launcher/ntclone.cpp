#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstring>

static void* map_fresh_ntdll() {
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\ntdll.dll");
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE m = CreateFileMappingW(f, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    void* view = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(m); CloseHandle(f);
    return view;
}

void* nt_get(const char* name) {
    static void* clean = map_fresh_ntdll();
    auto* dos = (IMAGE_DOS_HEADER*)clean;
    auto* nth = (IMAGE_NT_HEADERS*)((uint8_t*)clean + dos->e_lfanew);
    auto& dir = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto* exp = (IMAGE_EXPORT_DIRECTORY*)((uint8_t*)clean + dir.VirtualAddress);
    auto* names = (DWORD*)((uint8_t*)clean + exp->AddressOfNames);
    auto* ords  = (WORD*)((uint8_t*)clean + exp->AddressOfNameOrdinals);
    auto* funcs = (DWORD*)((uint8_t*)clean + exp->AddressOfFunctions);

    HMODULE live = GetModuleHandleW(L"ntdll.dll");
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* n = (const char*)clean + names[i];
        if (strcmp(n, name) == 0) {
            void* live_fn = (uint8_t*)live + funcs[ords[i]];
            void* clean_fn = (uint8_t*)clean + funcs[ords[i]];
            DWORD old;
            VirtualProtect(live_fn, 32, PAGE_EXECUTE_READWRITE, &old);
            memcpy(live_fn, clean_fn, 32);
            VirtualProtect(live_fn, 32, old, &old);
            FlushInstructionCache(GetCurrentProcess(), live_fn, 32);
            return live_fn;
        }
    }
    return nullptr;
}