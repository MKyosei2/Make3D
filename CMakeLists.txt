cmake_minimum_required(VERSION 3.20)
project(Make3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Make3DAdvancedCore STATIC
    src/Make3DAdvancedCore.cpp
    src/Make3DGuiAdapter.cpp
    src/Make3DMeshTools.cpp
    src/Make3DGltfMaterialExporter.cpp
    src/Make3DVertexColorGltfExporter.cpp
    src/Make3DHeroSemanticGltfExporter.cpp
    src/Make3DShapeInference.cpp
    src/Make3DLearnedShapeModel.cpp
    src/Make3DLearnedShapeTrainer.cpp
    src/Make3DHeroCharacterModel.cpp
    src/Make3DHeroDetailEnhancer.cpp
    src/Make3DHeroFineDetailPass.cpp
    src/Make3DModelPolisher.cpp
    src/Make3DMaskRefiner.cpp
    src/Make3DVoxelVolume.cpp
    src/Make3DProductionPipeline.cpp
    src/Make3DGameAssetPipeline.cpp
)

target_include_directories(Make3DAdvancedCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)

function(make3d_exe target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Make3DAdvancedCore)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

make3d_exe(Make3DAdvancedCLI src/Make3DAdvancedCLI.cpp)
make3d_exe(Make3DGenerateAdvancedSamples tools/GenerateAdvancedSamples.cpp)
make3d_exe(Make3DGenerateMeshQualityReport tools/GenerateMeshQualityReport.cpp)
make3d_exe(Make3DGenerateProductionPipelineSample tools/GenerateProductionPipelineSample.cpp)
make3d_exe(Make3DTrainLearnedShapeModel tools/TrainLearnedShapeModel.cpp)
make3d_exe(Make3DAdvancedCoreSmokeTest tests/Make3DAdvancedCoreSmokeTest.cpp)
make3d_exe(Make3DMeshToolsTest tests/Make3DMeshToolsTest.cpp)
make3d_exe(Make3DGltfMaterialExporterTest tests/Make3DGltfMaterialExporterTest.cpp)
make3d_exe(Make3DVertexColorGltfExporterTest tests/Make3DVertexColorGltfExporterTest.cpp)
make3d_exe(Make3DHeroSemanticGltfExporterTest tests/Make3DHeroSemanticGltfExporterTest.cpp)
make3d_exe(Make3DHeroOnlyProductionTest tests/Make3DHeroOnlyProductionTest.cpp)
make3d_exe(Make3DShapeInferenceTest tests/Make3DShapeInferenceTest.cpp)
make3d_exe(Make3DLearnedShapeModelTest tests/Make3DLearnedShapeModelTest.cpp)
make3d_exe(Make3DLearnedShapeTrainerTest tests/Make3DLearnedShapeTrainerTest.cpp)
make3d_exe(Make3DHeroCharacterModelTest tests/Make3DHeroCharacterModelTest.cpp)
make3d_exe(Make3DModelPolisherTest tests/Make3DModelPolisherTest.cpp)
make3d_exe(Make3DMaskRefinerTest tests/Make3DMaskRefinerTest.cpp)
make3d_exe(Make3DVoxelVolumeTest tests/Make3DVoxelVolumeTest.cpp)
make3d_exe(Make3DGameAssetPipelineTest tests/Make3DGameAssetPipelineTest.cpp)

if (WIN32)
    make3d_exe(Make3DAdvancedGui src/Make3DAdvancedGui.cpp)
    set_target_properties(Make3DAdvancedGui PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(Make3DAdvancedGui PRIVATE comctl32 comdlg32 shell32 ole32)
endif()

if (MSVC)
    target_compile_options(Make3DAdvancedCore PRIVATE /W4 /permissive-)
else()
    target_compile_options(Make3DAdvancedCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
foreach(test_name IN ITEMS
    Make3DAdvancedCoreSmokeTest
    Make3DMeshToolsTest
    Make3DGltfMaterialExporterTest
    Make3DVertexColorGltfExporterTest
    Make3DHeroSemanticGltfExporterTest
    Make3DHeroOnlyProductionTest
    Make3DShapeInferenceTest
    Make3DLearnedShapeModelTest
    Make3DLearnedShapeTrainerTest
    Make3DHeroCharacterModelTest
    Make3DModelPolisherTest
    Make3DMaskRefinerTest
    Make3DVoxelVolumeTest
    Make3DGameAssetPipelineTest)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_test(NAME Make3DGenerateAdvancedSamples COMMAND Make3DGenerateAdvancedSamples ${CMAKE_CURRENT_BINARY_DIR}/generated_samples)
add_test(NAME Make3DGenerateMeshQualityReport COMMAND Make3DGenerateMeshQualityReport ${CMAKE_CURRENT_BINARY_DIR}/mesh_quality)
add_test(NAME Make3DGenerateProductionPipelineSample COMMAND Make3DGenerateProductionPipelineSample ${CMAKE_CURRENT_BINARY_DIR}/production_pipeline)

add_test(NAME Make3DVerifyProductionSampleArtifact
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/production_pipeline
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyProductionSample.cmake)
set_tests_properties(Make3DVerifyProductionSampleArtifact PROPERTIES DEPENDS Make3DGenerateProductionPipelineSample)
add_test(NAME Make3DTrainLearnedShapeModel COMMAND Make3DTrainLearnedShapeModel ${CMAKE_CURRENT_BINARY_DIR}/learned_shape_training)

install(TARGETS Make3DAdvancedCLI Make3DGenerateAdvancedSamples Make3DGenerateMeshQualityReport Make3DGenerateProductionPipelineSample Make3DTrainLearnedShapeModel RUNTIME DESTINATION bin)
if (TARGET Make3DAdvancedGui)
    install(TARGETS Make3DAdvancedGui RUNTIME DESTINATION bin)
endif()
install(FILES README.md DESTINATION .)
install(DIRECTORY docs DESTINATION .)
