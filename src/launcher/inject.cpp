#include <windows.h>
#include <winternl.h>
#include <vector>
#include <cstdint>
#include <fstream>
#include <cstring>

extern void* nt_get(const char*);

using NtAllocateVirtualMemory_t = NTSTATUS(NTAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
using NtProtectVirtualMemory_t  = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
using NtWriteVirtualMemory_t    = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
using NtCreateThreadEx_t        = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);

static std::vector<uint8_t> read_file(const wchar_t* p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> v((size_t)sz);
    f.read((char*)v.data(), sz);
    return v;
}

static const uint8_t k_stub[] = {
    0x48, 0x83, 0xEC, 0x28,
    0x48, 0xB9, 0,0,0,0,0,0,0,0,
    0xBA, 0x01, 0x00, 0x00, 0x00,
    0x4D, 0x31, 0xC0,
    0x48, 0xB8, 0,0,0,0,0,0,0,0,
    0xFF, 0xD0,
    0x48, 0x83, 0xC4, 0x28,
    0xC3
};

bool manual_map(DWORD pid, const wchar_t* dll_path) {
    auto NtAlloc   = (NtAllocateVirtualMemory_t)nt_get("NtAllocateVirtualMemory");
    auto NtProtect = (NtProtectVirtualMemory_t)nt_get("NtProtectVirtualMemory");
    auto NtWrite   = (NtWriteVirtualMemory_t)nt_get("NtWriteVirtualMemory");
    auto NtThread  = (NtCreateThreadEx_t)nt_get("NtCreateThreadEx");

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) return false;

    auto bytes = read_file(dll_path);
    if (bytes.empty()) { CloseHandle(proc); return false; }

    auto* dos = (IMAGE_DOS_HEADER*)bytes.data();
    auto* nth = (IMAGE_NT_HEADERS64*)(bytes.data() + dos->e_lfanew);
    SIZE_T img_sz = nth->OptionalHeader.SizeOfImage;

    PVOID remote = nullptr;
    NtAlloc(proc, &remote, 0, &img_sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    std::vector<uint8_t> local(img_sz, 0);
    memcpy(local.data(), bytes.data(), nth->OptionalHeader.SizeOfHeaders);
    auto* sec = IMAGE_FIRST_SECTION(nth);
    for (WORD i = 0; i < nth->FileHeader.NumberOfSections; ++i, ++sec)
        if (sec->SizeOfRawData)
            memcpy(local.data() + sec->VirtualAddress,
                   bytes.data() + sec->PointerToRawData,
                   sec->SizeOfRawData);

    uint64_t delta = (uint64_t)remote - nth->OptionalHeader.ImageBase;
    auto& reloc_dir = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (reloc_dir.Size && delta) {
        auto* rb = (IMAGE_BASE_RELOCATION*)(local.data() + reloc_dir.VirtualAddress);
        auto  re = (IMAGE_BASE_RELOCATION*)((uint8_t*)rb + reloc_dir.Size);
        while (rb < re && rb->SizeOfBlock) {
            DWORD count = (rb->SizeOfBlock - sizeof *rb) / 2;
            auto* list = (WORD*)(rb + 1);
            for (DWORD k = 0; k < count; ++k) {
                if ((list[k] >> 12) == IMAGE_REL_BASED_DIR64) {
                    auto* p = (uint64_t*)(local.data() + rb->VirtualAddress + (list[k] & 0xFFF));
                    *p += delta;
                }
            }
            rb = (IMAGE_BASE_RELOCATION*)((uint8_t*)rb + rb->SizeOfBlock);
        }
    }

    auto& imp_dir = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imp_dir.Size) {
        auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)(local.data() + imp_dir.VirtualAddress);
        for (; imp->Name; ++imp) {
            HMODULE mod = LoadLibraryA((char*)(local.data() + imp->Name));
            auto* orig = (IMAGE_THUNK_DATA*)(local.data() + imp->OriginalFirstThunk);
            auto* iat  = (IMAGE_THUNK_DATA*)(local.data() + imp->FirstThunk);
            for (; orig->u1.AddressOfData; ++orig, ++iat) {
                auto* bn = (IMAGE_IMPORT_BY_NAME*)(local.data() + orig->u1.AddressOfData);
                iat->u1.Function = (ULONGLONG)GetProcAddress(mod, (char*)bn->Name);
            }
        }
    }

    SIZE_T wrote = 0;
    NtWrite(proc, remote, local.data(), img_sz, &wrote);

    sec = IMAGE_FIRST_SECTION(nth);
    for (WORD i = 0; i < nth->FileHeader.NumberOfSections; ++i, ++sec) {
        ULONG prot = PAGE_READONLY;
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) prot = PAGE_EXECUTE_READ;
        else if (sec->Characteristics & IMAGE_SCN_MEM_WRITE) prot = PAGE_READWRITE;
        PVOID p = (uint8_t*)remote + sec->VirtualAddress;
        SIZE_T s = sec->Misc.VirtualSize;
        ULONG old = 0;
        NtProtect(proc, &p, &s, prot, &old);
    }

    PVOID tramp = nullptr; SIZE_T tsz = sizeof k_stub;
    NtAlloc(proc, &tramp, 0, &tsz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint8_t stub[sizeof k_stub]; memcpy(stub, k_stub, sizeof k_stub);
    *(uint64_t*)(stub + 6)  = (uint64_t)remote;
    *(uint64_t*)(stub + 23) = (uint64_t)remote + nth->OptionalHeader.AddressOfEntryPoint;
    NtWrite(proc, tramp, stub, sizeof stub, &wrote);
    ULONG old = 0;
    NtProtect(proc, &tramp, &tsz, PAGE_EXECUTE_READ, &old);

    HANDLE th = nullptr;
    constexpr ULONG HIDE_FROM_DBG = 0x4;
    NtThread(&th, THREAD_ALL_ACCESS, nullptr, proc, tramp, nullptr,
             HIDE_FROM_DBG, 0, 0, 0, nullptr);

    if (th) { WaitForSingleObject(th, 5000); CloseHandle(th); }
    CloseHandle(proc);
    return true;
}