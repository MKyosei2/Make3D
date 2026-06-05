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
    src/Make3DAssetTypes.cpp
    src/Make3DGameAssetGenerator.cpp
    src/Make3DMeshRepairAndTexture.cpp
    src/Make3DCompleteGameAssetPipeline.cpp
    src/Make3DTypedProceduralAssets.cpp
    src/Make3DProductionPipeline.cpp
)

target_include_directories(Make3DAdvancedCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(Make3DAdvancedCLI
    src/Make3DAdvancedCLI.cpp
)

target_link_libraries(Make3DAdvancedCLI PRIVATE Make3DAdvancedCore)

add_executable(Make3DGameAssetCLI
    src/Make3DGameAssetCLI.cpp
)

target_link_libraries(Make3DGameAssetCLI PRIVATE Make3DAdvancedCore)

add_executable(Make3DTypedAssetCLI
    src/Make3DTypedAssetCLI.cpp
)

target_link_libraries(Make3DTypedAssetCLI PRIVATE Make3DAdvancedCore)

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

add_executable(Make3DGenerateProductionPipelineSample
    tools/GenerateProductionPipelineSample.cpp
)

target_link_libraries(Make3DGenerateProductionPipelineSample PRIVATE Make3DAdvancedCore)

add_executable(Make3DGenerateGenericAssetSample
    tools/GenerateGenericAssetSample.cpp
)

target_link_libraries(Make3DGenerateGenericAssetSample PRIVATE Make3DAdvancedCore)

add_executable(Make3DGenerateCompleteGameAssetSuite
    tools/GenerateCompleteGameAssetSuite.cpp
)

target_link_libraries(Make3DGenerateCompleteGameAssetSuite PRIVATE Make3DAdvancedCore)

add_executable(Make3DTrainLearnedShapeModel
    tools/TrainLearnedShapeModel.cpp
)

target_link_libraries(Make3DTrainLearnedShapeModel PRIVATE Make3DAdvancedCore)

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

add_executable(Make3DVertexColorGltfExporterTest
    tests/Make3DVertexColorGltfExporterTest.cpp
)

target_link_libraries(Make3DVertexColorGltfExporterTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DHeroSemanticGltfExporterTest
    tests/Make3DHeroSemanticGltfExporterTest.cpp
)

target_link_libraries(Make3DHeroSemanticGltfExporterTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DHeroOnlyProductionTest
    tests/Make3DHeroOnlyProductionTest.cpp
)

target_link_libraries(Make3DHeroOnlyProductionTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DShapeInferenceTest
    tests/Make3DShapeInferenceTest.cpp
)

target_link_libraries(Make3DShapeInferenceTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DLearnedShapeModelTest
    tests/Make3DLearnedShapeModelTest.cpp
)

target_link_libraries(Make3DLearnedShapeModelTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DLearnedShapeTrainerTest
    tests/Make3DLearnedShapeTrainerTest.cpp
)

target_link_libraries(Make3DLearnedShapeTrainerTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DHeroCharacterModelTest
    tests/Make3DHeroCharacterModelTest.cpp
)

target_link_libraries(Make3DHeroCharacterModelTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DModelPolisherTest
    tests/Make3DModelPolisherTest.cpp
)

target_link_libraries(Make3DModelPolisherTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DMaskRefinerTest
    tests/Make3DMaskRefinerTest.cpp
)

target_link_libraries(Make3DMaskRefinerTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DVoxelVolumeTest
    tests/Make3DVoxelVolumeTest.cpp
)

target_link_libraries(Make3DVoxelVolumeTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DGameAssetTest
    tests/Make3DGameAssetTest.cpp
)

target_link_libraries(Make3DGameAssetTest PRIVATE Make3DAdvancedCore)

add_executable(Make3DTypedProceduralAssetsTest
    tests/Make3DTypedProceduralAssetsTest.cpp
)

target_link_libraries(Make3DTypedProceduralAssetsTest PRIVATE Make3DAdvancedCore)

if (MSVC)
    target_compile_options(Make3DAdvancedCore PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCLI PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGameAssetCLI PRIVATE /W4 /permissive-)
    target_compile_options(Make3DTypedAssetCLI PRIVATE /W4 /permissive-)
    if (TARGET Make3DAdvancedGui)
        target_compile_options(Make3DAdvancedGui PRIVATE /W4 /permissive-)
    endif()
    target_compile_options(Make3DGenerateAdvancedSamples PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGenerateMeshQualityReport PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGenerateProductionPipelineSample PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGenerateGenericAssetSample PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGenerateCompleteGameAssetSuite PRIVATE /W4 /permissive-)
    target_compile_options(Make3DTrainLearnedShapeModel PRIVATE /W4 /permissive-)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DMeshToolsTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGltfMaterialExporterTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DVertexColorGltfExporterTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DHeroSemanticGltfExporterTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DHeroOnlyProductionTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DShapeInferenceTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DLearnedShapeModelTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DLearnedShapeTrainerTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DHeroCharacterModelTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DModelPolisherTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DMaskRefinerTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DVoxelVolumeTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DGameAssetTest PRIVATE /W4 /permissive-)
    target_compile_options(Make3DTypedProceduralAssetsTest PRIVATE /W4 /permissive-)
else()
    target_compile_options(Make3DAdvancedCore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCLI PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGameAssetCLI PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DTypedAssetCLI PRIVATE -Wall -Wextra -Wpedantic)
    if (TARGET Make3DAdvancedGui)
        target_compile_options(Make3DAdvancedGui PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_options(Make3DGenerateAdvancedSamples PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGenerateMeshQualityReport PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGenerateProductionPipelineSample PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGenerateGenericAssetSample PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGenerateCompleteGameAssetSuite PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DTrainLearnedShapeModel PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DAdvancedCoreSmokeTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DMeshToolsTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGltfMaterialExporterTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DVertexColorGltfExporterTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DHeroSemanticGltfExporterTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DHeroOnlyProductionTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DShapeInferenceTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DLearnedShapeModelTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DLearnedShapeTrainerTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DHeroCharacterModelTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DModelPolisherTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DMaskRefinerTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DVoxelVolumeTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DGameAssetTest PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(Make3DTypedProceduralAssetsTest PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME Make3DAdvancedCoreSmokeTest COMMAND Make3DAdvancedCoreSmokeTest)
add_test(NAME Make3DMeshToolsTest COMMAND Make3DMeshToolsTest)
add_test(NAME Make3DGltfMaterialExporterTest COMMAND Make3DGltfMaterialExporterTest)
add_test(NAME Make3DVertexColorGltfExporterTest COMMAND Make3DVertexColorGltfExporterTest)
add_test(NAME Make3DHeroSemanticGltfExporterTest COMMAND Make3DHeroSemanticGltfExporterTest)
add_test(NAME Make3DHeroOnlyProductionTest COMMAND Make3DHeroOnlyProductionTest)
add_test(NAME Make3DShapeInferenceTest COMMAND Make3DShapeInferenceTest)
add_test(NAME Make3DLearnedShapeModelTest COMMAND Make3DLearnedShapeModelTest)
add_test(NAME Make3DLearnedShapeTrainerTest COMMAND Make3DLearnedShapeTrainerTest)
add_test(NAME Make3DHeroCharacterModelTest COMMAND Make3DHeroCharacterModelTest)
add_test(NAME Make3DModelPolisherTest COMMAND Make3DModelPolisherTest)
add_test(NAME Make3DMaskRefinerTest COMMAND Make3DMaskRefinerTest)
add_test(NAME Make3DVoxelVolumeTest COMMAND Make3DVoxelVolumeTest)
add_test(NAME Make3DGameAssetTest COMMAND Make3DGameAssetTest)
add_test(NAME Make3DTypedProceduralAssetsTest COMMAND Make3DTypedProceduralAssetsTest)
add_test(NAME Make3DGameAssetCLIHelp COMMAND Make3DGameAssetCLI --help)
add_test(NAME Make3DTypedAssetCLIHelp COMMAND Make3DTypedAssetCLI --help)
add_test(NAME Make3DGenerateAdvancedSamples COMMAND Make3DGenerateAdvancedSamples ${CMAKE_CURRENT_BINARY_DIR}/generated_samples)
add_test(NAME Make3DGenerateMeshQualityReport COMMAND Make3DGenerateMeshQualityReport ${CMAKE_CURRENT_BINARY_DIR}/mesh_quality)
add_test(NAME Make3DGenerateProductionPipelineSample COMMAND Make3DGenerateProductionPipelineSample ${CMAKE_CURRENT_BINARY_DIR}/production_pipeline)
add_test(NAME Make3DGenerateGenericAssetSample COMMAND Make3DGenerateGenericAssetSample ${CMAKE_CURRENT_BINARY_DIR}/generic_asset)
add_test(NAME Make3DGenerateCompleteGameAssetSuite COMMAND Make3DGenerateCompleteGameAssetSuite ${CMAKE_CURRENT_BINARY_DIR}/complete_game_asset_suite)
add_test(NAME Make3DGenerateTypedFurniture COMMAND Make3DTypedAssetCLI --type furniture --output ${CMAKE_CURRENT_BINARY_DIR}/typed_furniture)
add_test(NAME Make3DGenerateTypedMachine COMMAND Make3DTypedAssetCLI --type machine --output ${CMAKE_CURRENT_BINARY_DIR}/typed_machine)
add_test(NAME Make3DGenerateTypedTool COMMAND Make3DTypedAssetCLI --type tool --output ${CMAKE_CURRENT_BINARY_DIR}/typed_tool)
add_test(NAME Make3DGenerateTypedTerrain COMMAND Make3DTypedAssetCLI --type terrain --output ${CMAKE_CURRENT_BINARY_DIR}/typed_terrain)
add_test(NAME Make3DVerifyProductionSampleArtifact
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/production_pipeline
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyProductionSample.cmake)
set_tests_properties(Make3DVerifyProductionSampleArtifact PROPERTIES DEPENDS Make3DGenerateProductionPipelineSample)
add_test(NAME Make3DTrainLearnedShapeModel COMMAND Make3DTrainLearnedShapeModel ${CMAKE_CURRENT_BINARY_DIR}/learned_shape_training)

install(TARGETS Make3DAdvancedCLI Make3DGameAssetCLI Make3DTypedAssetCLI Make3DGenerateAdvancedSamples Make3DGenerateMeshQualityReport Make3DGenerateProductionPipelineSample Make3DGenerateGenericAssetSample Make3DGenerateCompleteGameAssetSuite Make3DTrainLearnedShapeModel RUNTIME DESTINATION bin)
if (TARGET Make3DAdvancedGui)
    install(TARGETS Make3DAdvancedGui RUNTIME DESTINATION bin)
endif()
install(FILES README.md DESTINATION .)
install(DIRECTORY docs DESTINATION .)

# The original Win32 GUI is still kept in MainApp.cpp.
# Make3DAdvancedGui is a smaller GUI that directly uses the advanced reconstruction backend.
