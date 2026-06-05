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
    src/Make3DShapeInference.cpp
    src/Make3DLearnedShapeModel.cpp
    src/Make3DLearnedShapeTrainer.cpp
    src/Make3DMaskRefiner.cpp
    src/Make3DAssetTypes.cpp
    src/Make3DGameAssetGenerator.cpp
    src/Make3DMeshRepairAndTexture.cpp
    src/Make3DMeshQualityGate.cpp
    src/Make3DCompleteGameAssetPipeline.cpp
    src/Make3DTypedProceduralAssets.cpp
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

make3d_exe(Make3D src/Make3DAdvancedCLI.cpp)
make3d_exe(Make3DAdvancedCLI src/Make3DAdvancedCLI.cpp)
make3d_exe(Make3DGenerateProductionPipelineSample tools/GenerateProductionPipelineSample.cpp)
make3d_exe(Make3DGenerateGenericAssetSample tools/GenerateGenericAssetSample.cpp)
make3d_exe(Make3DGenerateCompleteGameAssetSuite tools/GenerateCompleteGameAssetSuite.cpp)
make3d_exe(Make3DTrainLearnedShapeModel tools/TrainLearnedShapeModel.cpp)
make3d_exe(Make3DAdvancedCoreSmokeTest tests/Make3DAdvancedCoreSmokeTest.cpp)
make3d_exe(Make3DMeshToolsTest tests/Make3DMeshToolsTest.cpp)
make3d_exe(Make3DGltfMaterialExporterTest tests/Make3DGltfMaterialExporterTest.cpp)
make3d_exe(Make3DVertexColorGltfExporterTest tests/Make3DVertexColorGltfExporterTest.cpp)
make3d_exe(Make3DHeroOnlyProductionTest tests/Make3DHeroOnlyProductionTest.cpp)
make3d_exe(Make3DShapeInferenceTest tests/Make3DShapeInferenceTest.cpp)
make3d_exe(Make3DLearnedShapeModelTest tests/Make3DLearnedShapeModelTest.cpp)
make3d_exe(Make3DLearnedShapeTrainerTest tests/Make3DLearnedShapeTrainerTest.cpp)
make3d_exe(Make3DMaskRefinerTest tests/Make3DMaskRefinerTest.cpp)
make3d_exe(Make3DGameAssetPipelineTest tests/Make3DGameAssetPipelineTest.cpp)
make3d_exe(Make3DMeshQualityGateTest tests/Make3DMeshQualityGateTest.cpp)

add_executable(Make3DGameAssetCLI
    src/Make3DGameAssetCLI.cpp
)

target_link_libraries(Make3DGameAssetCLI PRIVATE Make3DAdvancedCore)

add_executable(Make3DTypedAssetCLI
    src/Make3DTypedAssetCLI.cpp
)

target_link_libraries(Make3DTypedAssetCLI PRIVATE Make3DAdvancedCore)

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
    Make3DHeroOnlyProductionTest
    Make3DShapeInferenceTest
    Make3DLearnedShapeModelTest
    Make3DLearnedShapeTrainerTest
    Make3DMaskRefinerTest
    Make3DGameAssetPipelineTest
    Make3DMeshQualityGateTest)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

add_test(NAME Make3DGenerateProductionPipelineSample COMMAND Make3DGenerateProductionPipelineSample ${CMAKE_CURRENT_BINARY_DIR}/production_pipeline)
add_test(NAME Make3DVerifyProductionSampleArtifact
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/production_pipeline
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyProductionSample.cmake)
set_tests_properties(Make3DVerifyProductionSampleArtifact PROPERTIES DEPENDS Make3DGenerateProductionPipelineSample)

add_test(NAME Make3DGenerateGenericAssetSample COMMAND Make3DGenerateGenericAssetSample ${CMAKE_CURRENT_BINARY_DIR}/generic_asset)
add_test(NAME Make3DGenerateCompleteGameAssetSuite COMMAND Make3DGenerateCompleteGameAssetSuite ${CMAKE_CURRENT_BINARY_DIR}/complete_game_asset_suite)
add_test(NAME Make3DGenerateTypedFurniture COMMAND Make3DTypedAssetCLI --type furniture --output ${CMAKE_CURRENT_BINARY_DIR}/typed_assets/furniture)
add_test(NAME Make3DGenerateTypedMachine COMMAND Make3DTypedAssetCLI --type machine --output ${CMAKE_CURRENT_BINARY_DIR}/typed_assets/machine)
add_test(NAME Make3DGenerateTypedTool COMMAND Make3DTypedAssetCLI --type tool --output ${CMAKE_CURRENT_BINARY_DIR}/typed_assets/tool)
add_test(NAME Make3DGenerateTypedTerrain COMMAND Make3DTypedAssetCLI --type terrain --output ${CMAKE_CURRENT_BINARY_DIR}/typed_assets/terrain)
add_test(NAME Make3DVerifyGameAssetArtifacts
    COMMAND ${CMAKE_COMMAND}
        -DGENERIC_ASSET_DIR=${CMAKE_CURRENT_BINARY_DIR}/generic_asset
        -DCOMPLETE_SUITE_DIR=${CMAKE_CURRENT_BINARY_DIR}/complete_game_asset_suite
        -DTYPED_ASSET_DIR=${CMAKE_CURRENT_BINARY_DIR}/typed_assets
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyGameAssetArtifacts.cmake)
set_tests_properties(Make3DVerifyGameAssetArtifacts PROPERTIES DEPENDS "Make3DGenerateGenericAssetSample;Make3DGenerateCompleteGameAssetSuite;Make3DGenerateTypedFurniture;Make3DGenerateTypedMachine;Make3DGenerateTypedTool;Make3DGenerateTypedTerrain")
add_test(NAME Make3DTrainLearnedShapeModel COMMAND Make3DTrainLearnedShapeModel ${CMAKE_CURRENT_BINARY_DIR}/learned_shape_training)

install(TARGETS Make3D Make3DAdvancedCLI Make3DGameAssetCLI Make3DTypedAssetCLI Make3DGenerateProductionPipelineSample Make3DGenerateGenericAssetSample Make3DGenerateCompleteGameAssetSuite Make3DTrainLearnedShapeModel RUNTIME DESTINATION bin)
if (TARGET Make3DAdvancedGui)
    install(TARGETS Make3DAdvancedGui RUNTIME DESTINATION bin)
endif()
install(FILES README.md DESTINATION .)
install(DIRECTORY docs DESTINATION .)
