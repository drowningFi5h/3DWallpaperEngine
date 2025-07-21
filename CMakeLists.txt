cmake_minimum_required(VERSION 3.20)
project(ENGINE)

set(CMAKE_CXX_STANDARD 20)

# Use this DirectX path for include directories
set(DIRECTX_DIR "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um")

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create executable (use consistent naming with run.bat)
add_executable(WallpaperENGINE WIN32 main.cpp)

# Libraries
target_link_libraries(WallpaperENGINE
        kernel32.lib
        d3d11.lib
        d3dcompiler.lib
        dxgi.lib
        dxguid.lib
)

# Copy shader file to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl
        ${CMAKE_CURRENT_BINARY_DIR}/shader.hlsl COPYONLY)

# Copy wallpaper texture to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Wallpapers/
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Wallpapers/)

# Python script target
add_custom_target(
        RunPythonScript
        COMMAND python python/face_tracker.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)