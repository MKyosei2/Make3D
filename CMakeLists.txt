cmake_minimum_required(VERSION 3.20)
project(Make3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Make3DAdvancedCLI
    src/Make3DAdvancedCLI.cpp
    src/Make3DAdvancedCore.cpp
)

target_include_directories(Make3DAdvancedCLI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (MSVC)
    target_compile_options(Make3DAdvancedCLI PRIVATE /W4 /permissive-)
else()
    target_compile_options(Make3DAdvancedCLI PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The existing Win32 GUI is still kept in MainApp.cpp.
# This CMake target builds the new standalone reconstruction engine first,
# so the algorithm can be validated from the command line before it is wired into the GUI.
