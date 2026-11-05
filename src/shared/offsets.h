#pragma once
#include <cstdint>

struct RobloxOffsets {
    uint32_t ScriptContext_push_pattern;
    uint32_t DataModel_getPtr;
    uint32_t TaskScheduler_singleton;
    uint32_t Instance_className_vfunc_idx;
    uint32_t Instance_children_vec;
    uint32_t Instance_name;
    uint32_t Script_source_proto;
    uint32_t ModuleScript_source_proto;
    uint32_t Proto_k;
    uint32_t Proto_sizek;
    uint32_t Proto_code;
    uint32_t Proto_sizecode;
    uint32_t Proto_source;
    uint32_t TString_data;
};

inline RobloxOffsets g_off = {
    0x0,
    0x0, 0x0, 0x8, 0x60, 0x78,
    0x40, 0x40,
    0x18, 0x20, 0x8, 0x10, 0x30, 0x18
};