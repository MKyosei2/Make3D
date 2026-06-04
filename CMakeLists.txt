cmake_minimum_required(VERSION 3.20)
project(Make3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Make3DAdvancedCore STATIC
    src/Make3DAdvancedCore.cpp
)

target_include_directories(Make3DAdvancedCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(Make3DAdvancedCLI
    src/Make3DAdvancedCLI.cpp
)

target_link_libraries(Make3DAdvancedCLI PRIVATE Make3DAdvancedCore)

add_executable(Make3DAdvancedCoreSmokeTest
    tests/Make3DAdvancedCoreSmokeTest.cpp
)

target_link_libraries(Make3DAdvancedCoreSmokeTest PRIVATE Make3DAdvancedCore)

if (MSVC)
    target_compile_options(Make3DAdvancedCore PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCLI PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE /W4 /permissive-)
else()
    target_compile_options(Make3DAdvancedCore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCLI PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME Make3DAdvancedCoreSmokeTest COMMAND Make3DAdvancedCoreSmokeTest)

# The existing Win32 GUI is still kept in MainApp.cpp.
# This CMake target builds and tests the new standalone reconstruction engine first,
# so the algorithm can be validated from the command line before it is wired into the GUI.
