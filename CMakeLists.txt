cmake_minimum_required(VERSION 3.21)
project(pixel_wars LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Sources -------------------------------------------------------------
# Explicit list beats globbing: new files must be added here on purpose.
set(PIXEL_WARS_SOURCES
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
    src/game.cpp
)

add_executable(pixel_wars WIN32 ${PIXEL_WARS_SOURCES})

target_include_directories(pixel_wars PRIVATE src)

# --- Warnings / build flags ----------------------------------------------
target_compile_options(pixel_wars PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# NDEBUG only for release configs, _DEBUG only for debug.
target_compile_definitions(pixel_wars PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# --- DirectX 11 (ships with the Windows SDK) ------------------------------
# No find_package needed: these are import libraries resolved by the MSVC
# toolchain against the installed Windows SDK.
target_link_libraries(pixel_wars PRIVATE
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
)
