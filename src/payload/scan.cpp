#include "offsets.h"
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <cstdint>

#pragma comment(lib, "psapi.lib")

namespace {

struct Range { uint8_t* base; size_t size; };

Range text_of_main() {
    HMODULE m = GetModuleHandleW(nullptr);
    MODULEINFO mi{};
    GetModuleInformation(GetCurrentProcess(), m, &mi, sizeof mi);
    auto* dos = (IMAGE_DOS_HEADER*)m;
    auto* nth = (IMAGE_NT_HEADERS64*)((uint8_t*)m + dos->e_lfanew);
    auto* s = IMAGE_FIRST_SECTION(nth);
    for (WORD i = 0; i < nth->FileHeader.NumberOfSections; ++i, ++s)
        if (memcmp(s->Name, ".text", 5) == 0)
            return { (uint8_t*)m + s->VirtualAddress, s->Misc.VirtualSize };
    return { nullptr, 0 };
}

uint8_t* find_sig(Range r, const char* pat, const char* mask) {
    size_t plen = strlen(mask);
    for (size_t i = 0; i + plen < r.size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < plen; ++j) {
            if (mask[j] == 'x' && r.base[i + j] != (uint8_t)pat[j]) { ok = false; break; }
        }
        if (ok) return r.base + i;
    }
    return nullptr;
}

uint8_t* find_str(Range r, const char* s) {
    size_t slen = strlen(s);
    for (size_t i = 0; i + slen < r.size; ++i) {
        if (memcmp(r.base + i, s, slen) == 0) return r.base + i;
    }
    return nullptr;
}

} // anon

namespace scan {

bool discover_offsets() {
    Range r = text_of_main();
    if (!r.base) return false;

    uint8_t* hit = find_sig(r,
        "\x48\x8D\x05\x00\x00\x00\x00\x48\x89\x81\x00\x00\x00\x00",
        "xxx????xxx????");
    if (hit) g_off.ScriptContext_push_pattern = (uint32_t)(hit - r.base);

    return true;
}

void* find_script_context() {
    Range r = text_of_main();
    uint8_t* s = find_str(r, "RBX::ScriptContext");
    return s;
}

} // namespace scan