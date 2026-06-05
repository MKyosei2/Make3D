if(NOT DEFINED GENERIC_ASSET_DIR)
    message(FATAL_ERROR "GENERIC_ASSET_DIR is required")
endif()

if(NOT DEFINED COMPLETE_SUITE_DIR)
    message(FATAL_ERROR "COMPLETE_SUITE_DIR is required")
endif()

if(NOT DEFINED TYPED_ASSET_DIR)
    message(FATAL_ERROR "TYPED_ASSET_DIR is required")
endif()

set(generic_required_files
    "${GENERIC_ASSET_DIR}/OPEN_THIS_FIRST.txt"
    "${GENERIC_ASSET_DIR}/input_building.tga"
    "${GENERIC_ASSET_DIR}/output/make3d_game_asset.obj"
    "${GENERIC_ASSET_DIR}/output/make3d_game_asset.gltf"
    "${GENERIC_ASSET_DIR}/output/make3d_game_asset_collider.obj"
    "${GENERIC_ASSET_DIR}/output/make3d_game_asset_lod_proxy.obj"
    "${GENERIC_ASSET_DIR}/output/make3d_game_asset_report.md"
    "${GENERIC_ASSET_DIR}/output/make3d_game_asset_manifest.json"
)

foreach(path IN LISTS generic_required_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required generic asset artifact is missing: ${path}")
    endif()
endforeach()

set(complete_samples building vehicle prop)
foreach(sample IN LISTS complete_samples)
    set(sample_dir "${COMPLETE_SUITE_DIR}/${sample}/complete")
    set(complete_required_files
        "${sample_dir}/base/make3d_game_asset.obj"
        "${sample_dir}/base/make3d_game_asset.gltf"
        "${sample_dir}/base/make3d_game_asset_collider.obj"
        "${sample_dir}/base/make3d_game_asset_lod_proxy.obj"
        "${sample_dir}/projected_texture.ppm"
        "${sample_dir}/make3d_complete_textured.obj"
        "${sample_dir}/make3d_complete_textured.mtl"
        "${sample_dir}/complete_game_asset_report.md"
        "${sample_dir}/complete_game_asset_manifest.json"
        "${sample_dir}/IMPORT_GUIDE.md"
        "${sample_dir}/unity_import_metadata.json"
    )
    foreach(path IN LISTS complete_required_files)
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR "Required complete asset artifact is missing: ${path}")
        endif()
    endforeach()
endforeach()

set(typed_samples furniture machine tool terrain)
foreach(sample IN LISTS typed_samples)
    set(sample_dir "${TYPED_ASSET_DIR}/${sample}")
    set(typed_required_files
        "${sample_dir}/make3d_typed_asset.obj"
        "${sample_dir}/make3d_typed_asset.gltf"
        "${sample_dir}/make3d_typed_asset_report.md"
        "${sample_dir}/make3d_typed_asset_manifest.json"
    )
    foreach(path IN LISTS typed_required_files)
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR "Required typed asset artifact is missing: ${path}")
        endif()
    endforeach()
endforeach()

file(READ "${GENERIC_ASSET_DIR}/output/make3d_game_asset_manifest.json" generic_manifest)
if(NOT generic_manifest MATCHES "assetType")
    message(FATAL_ERROR "Generic asset manifest does not include assetType/classification metadata")
endif()

file(READ "${COMPLETE_SUITE_DIR}/building/complete/complete_game_asset_manifest.json" complete_manifest)
if(NOT complete_manifest MATCHES "texturedObj")
    message(FATAL_ERROR "Complete asset manifest does not include texturedObj")
endif()

file(READ "${TYPED_ASSET_DIR}/furniture/make3d_typed_asset_manifest.json" typed_manifest)
if(NOT typed_manifest MATCHES "Furniture")
    message(FATAL_ERROR "Typed furniture manifest does not identify Furniture")
endif()

message(STATUS "Generic game asset artifact contract verified")
