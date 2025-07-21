cmake_minimum_required(VERSION 3.28)
project(CHIP-8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(CHIP-8 main.cpp imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp rlImGui/rlImGui.cpp)

target_link_libraries(CHIP-8 raylib winmm)
target_include_directories(CHIP-8 PRIVATE SYSTEM raylib-5.5_win64_msvc16/include imgui rlImGui)
target_link_directories(CHIP-8 PRIVATE raylib-5.5_win64_msvc16/lib)
