cmake_minimum_required(VERSION 3.20)
project(Make3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Make3DAdvancedCore STATIC
    src/Make3DAdvancedCore.cpp
    src/Make3DGuiAdapter.cpp
)

target_include_directories(Make3DAdvancedCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(Make3DAdvancedCLI
    src/Make3DAdvancedCLI.cpp
)

target_link_libraries(Make3DAdvancedCLI PRIVATE Make3DAdvancedCore)

add_executable(Make3DAdvancedGui WIN32
    src/Make3DAdvancedGui.cpp
)

target_link_libraries(Make3DAdvancedGui PRIVATE Make3DAdvancedCore comctl32 shell32 ole32)

add_executable(Make3DGenerateAdvancedSamples
    tools/GenerateAdvancedSamples.cpp
)

target_link_libraries(Make3DGenerateAdvancedSamples PRIVATE Make3DAdvancedCore)

add_executable(Make3DAdvancedCoreSmokeTest
    tests/Make3DAdvancedCoreSmokeTest.cpp
)

target_link_libraries(Make3DAdvancedCoreSmokeTest PRIVATE Make3DAdvancedCore)

if (MSVC)
    target_compile_options(Make3DAdvancedCore PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCLI PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedGui PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGenerateAdvancedSamples PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE /W4 /permissive-)
else()
    target_compile_options(Make3DAdvancedCore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCLI PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedGui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGenerateAdvancedSamples PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME Make3DAdvancedCoreSmokeTest COMMAND Make3DAdvancedCoreSmokeTest)
add_test(NAME Make3DGenerateAdvancedSamples COMMAND Make3DGenerateAdvancedSamples ${CMAKE_CURRENT_BINARY_DIR}/generated_samples)

# The original Win32 GUI is still kept in MainApp.cpp.
# Make3DAdvancedGui is a smaller GUI that directly uses the advanced reconstruction backend.
