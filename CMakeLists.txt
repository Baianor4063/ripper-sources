cmake_minimum_required(VERSION 3.20)
project(roblox_ripper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/W4 /permissive- /GS- /guard:cf- /Zc:__cplusplus)
add_link_options(/DYNAMICBASE:NO /NXCOMPAT)

add_executable(ripper WIN32
    src/launcher/main.cpp
    src/launcher/gui.cpp
    src/launcher/inject.cpp
    src/launcher/ntclone.cpp
    src/launcher/etw_mute.cpp
    src/launcher/imgui/imgui.cpp
    src/launcher/imgui/imgui_draw.cpp
    src/launcher/imgui/imgui_tables.cpp
    src/launcher/imgui/imgui_widgets.cpp
    src/launcher/imgui/backends/imgui_impl_win32.cpp
    src/launcher/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(ripper PRIVATE src/launcher/imgui src/shared)
target_link_libraries(ripper PRIVATE d3d11 dxgi dwmapi shell32 user32)

add_library(payload SHARED
    src/payload/dllmain.cpp
    src/payload/scan.cpp
    src/payload/luau_walker.cpp
    src/payload/bytecode_decoder.cpp
    src/payload/dumper.cpp
)
target_include_directories(payload PRIVATE src/payload src/shared)
set_target_properties(payload PROPERTIES OUTPUT_NAME "payload" SUFFIX ".bin")
target_link_options(payload PRIVATE /ENTRY:DllMain /NODEFAULTLIB:libcmt /MERGE:.rdata=.text)