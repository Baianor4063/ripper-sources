#include "offsets.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

extern std::string proto_source(const void* proto_raw);
extern void walk(const void* inst, std::vector<const void*>& out_scripts);

static fs::path desktop_dump_root() {
    wchar_t p[MAX_PATH]; SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, 0, p);
    fs::path root = fs::path(p) / L"RobloxDump";
    fs::create_directories(root);
    return root;
}

static std::string inst_name(const void* inst) {
    auto* base = (const uint8_t*)inst;
    const char* s = *(const char**)(base + g_off.Instance_name);
    return s ? std::string(s) : std::string("unnamed");
}

static std::string place_id_str() {
    char buf[64];
    SYSTEMTIME t; GetLocalTime(&t);
    sprintf_s(buf, "run_%04u%02u%02u_%02u%02u%02u",
             t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return buf;
}

static void write_one(const fs::path& root, const void* script) {
    auto* base = (const uint8_t*)script;
    const void* proto = *(const void**)(base + g_off.Script_source_proto);
    if (!proto) return;

    std::string src = proto_source(proto);
    if (src.empty()) return;

    fs::path file = root / (inst_name(script) + ".lua");
    int n = 1;
    while (fs::exists(file)) {
        file = root / (inst_name(script) + "_" + std::to_string(n++) + ".lua");
    }
    std::ofstream f(file, std::ios::binary);
    f.write(src.data(), (std::streamsize)src.size());
}

namespace dumper {

void rip_all(void* sctx) {
    auto dm_ptr_at = (const uint8_t*)sctx + g_off.DataModel_getPtr;
    const void* dm = *(const void**)dm_ptr_at;
    if (!dm) return;

    std::vector<const void*> scripts;
    walk(dm, scripts);

    fs::path root = desktop_dump_root() / place_id_str();
    fs::create_directories(root);

    for (auto* s : scripts) write_one(root, s);
}

} // namespace dumper