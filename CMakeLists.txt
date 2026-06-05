cmake_minimum_required(VERSION 3.20)
project(Make3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Make3DAdvancedCore STATIC
    src/Make3DAdvancedCore.cpp
    src/Make3DGuiAdapter.cpp
    src/Make3DMeshTools.cpp
    src/Make3DGltfMaterialExporter.cpp
)

target_include_directories(Make3DAdvancedCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(Make3DAdvancedCLI
    src/Make3DAdvancedCLI.cpp
)

target_link_libraries(Make3DAdvancedCLI PRIVATE Make3DAdvancedCore)

if (WIN32)
    add_executable(Make3DAdvancedGui WIN32
        src/Make3DAdvancedGui.cpp
    )

    target_link_libraries(Make3DAdvancedGui PRIVATE
        Make3DAdvancedCore
        comctl32
        comdlg32
        shell32
        ole32
    )
endif()

add_executable(Make3DGenerateAdvancedSamples
    tools/GenerateAdvancedSamples.cpp
)

target_link_libraries(Make3DGenerateAdvancedSamples PRIVATE Make3DAdvancedCore)

add_executable(Make3DGenerateMeshQualityReport
    tools/GenerateMeshQualityReport.cpp
)

target_link_libraries(Make3DGenerateMeshQualityReport PRIVATE Make3DAdvancedCore)

add_executable(Make3DAdvancedCoreSmokeTest
    tests/Make3DAdvancedCoreSmokeTest.cpp
)

target_link_libraries(Make3DAdvancedCoreSmokeTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DMeshToolsTest
    tests/Make3DMeshToolsTest.cpp
)

target_link_libraries(Make3DMeshToolsTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DGltfMaterialExporterTest
    tests/Make3DGltfMaterialExporterTest.cpp
)

target_link_libraries(Make3DGltfMaterialExporterTest PRIVATE Make3DAdvancedCore)

if (MSVC)
    target_compile_options(Make3DAdvancedCore PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCLI PRIVATE /W4 /permissive-)
    if (TARGET Make3DAdvancedGui)
        target_compile_options(Make3DAdvancedGui PRIVATE /W4 /permissive-)
    endif()
    target_compile_options(Make3DGenerateAdvancedSamples PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGenerateMeshQualityReport PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DMeshToolsTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGltfMaterialExporterTest PRIVATE /W4 /permissive-)
else()
    target_compile_options(Make3DAdvancedCore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCLI PRIVATE -Wall -Wextra -Wpedantic)
    if (TARGET Make3DAdvancedGui)
        target_compile_options(Make3DAdvancedGui PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_options(Make3DGenerateAdvancedSamples PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGenerateMeshQualityReport PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DMeshToolsTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGltfMaterialExporterTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME Make3DAdvancedCoreSmokeTest COMMAND Make3DAdvancedCoreSmokeTest)
add_test(NAME Make3DMeshToolsTest COMMAND Make3DMeshToolsTest)
add_test(NAME Make3DGltfMaterialExporterTest COMMAND Make3DGltfMaterialExporterTest)
add_test(NAME Make3DGenerateAdvancedSamples COMMAND Make3DGenerateAdvancedSamples ${CMAKE_CURRENT_BINARY_DIR}/generated_samples)
add_test(NAME Make3DGenerateMeshQualityReport COMMAND Make3DGenerateMeshQualityReport ${CMAKE_CURRENT_BINARY_DIR}/mesh_quality)

install(TARGETS Make3DAdvancedCLI Make3DGenerateAdvancedSamples Make3DGenerateMeshQualityReport RUNTIME DESTINATION bin)
if (TARGET Make3DAdvancedGui)
    install(TARGETS Make3DAdvancedGui RUNTIME DESTINATION bin)
endif()
install(FILES README.md DESTINATION .)
install(DIRECTORY docs DESTINATION .)

# The original Win32 GUI is still kept in MainApp.cpp.
# Make3DAdvancedGui is a smaller GUI that directly uses the advanced reconstruction backend.
