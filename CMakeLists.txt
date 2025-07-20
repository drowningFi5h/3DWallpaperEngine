cmake_minimum_required(VERSION 3.31)
project(ENGINE)

set(CMAKE_CXX_STANDARD 20)

set(DIRECTX_DIR "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um")
add_executable(Wallpaper_ENGINE WIN32 main.cpp)

# Libraries
target_link_libraries(Wallpaper_ENGINE
    kernel32.lib
    d3d11.lib
    d3dcompiler.lib
    dxgi.lib
)

add_custom_target(
        RunPythonScript
        COMMAND python python/face_tracker.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
