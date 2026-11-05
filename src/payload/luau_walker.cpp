#include "offsets.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

struct TString { int len; char data[1]; };

std::string proto_source(const void* proto_raw) {
    auto* base = (const uint8_t*)proto_raw;
    auto* source_ts = *(TString**)(base + g_off.Proto_source);
    if (!source_ts) return {};
    return std::string(source_ts->data, source_ts->len);
}

void walk(const void* inst, std::vector<const void*>& out_scripts) {
    auto* base = (const uint8_t*)inst;
    auto* children_vec = base + g_off.Instance_children_vec;
    auto beg = *(const void**)(children_vec);
    auto end = *(const void**)(children_vec + sizeof(void*));
    size_t n = ((const void**)end - (const void**)beg);
    for (size_t i = 0; i < n; ++i) {
        const void* child = ((const void**)beg)[i];
        if (!child) continue;
        auto** vt = *(void***)child;
        using class_fn_t = const char*(__fastcall*)(const void*);
        auto class_fn = (class_fn_t)vt[g_off.Instance_className_vfunc_idx];
        const char* cn = class_fn(child);
        if (cn && (strcmp(cn, "LocalScript") == 0
                || strcmp(cn, "ModuleScript") == 0
                || strcmp(cn, "Script") == 0)) {
            out_scripts.push_back(child);
        }
        walk(child, out_scripts);
    }
}