if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

set(required_files
    "${OUTPUT_DIR}/OPEN_THIS_FIRST.txt"
    "${OUTPUT_DIR}/PORTFOLIO_EVIDENCE.md"
    "${OUTPUT_DIR}/input_noisy_character.tga"
    "${OUTPUT_DIR}/output/hero/make3d_hero_character.obj"
    "${OUTPUT_DIR}/output/hero/make3d_hero_character_material.gltf"
    "${OUTPUT_DIR}/output/hero/make3d_hero_character_vertex_color.gltf"
    "${OUTPUT_DIR}/output/game_asset/make3d_game_asset.obj"
    "${OUTPUT_DIR}/output/game_asset/make3d_game_asset.gltf"
    "${OUTPUT_DIR}/output/game_asset/make3d_game_asset_collider.obj"
    "${OUTPUT_DIR}/output/game_asset/make3d_game_asset_lod_proxy.obj"
    "${OUTPUT_DIR}/output/game_asset/make3d_game_asset_report.md"
    "${OUTPUT_DIR}/output/game_asset/make3d_game_asset_manifest.json"
    "${OUTPUT_DIR}/output/production_report.md"
    "${OUTPUT_DIR}/output/production_report.json"
    "${OUTPUT_DIR}/output/debug_mask_refined.ppm"
    "${OUTPUT_DIR}/output/debug_depth_learned.ppm"
)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required production sample artifact is missing: ${path}")
    endif()
endforeach()

set(forbidden_paths
    "${OUTPUT_DIR}/output/raw"
    "${OUTPUT_DIR}/output/polished"
    "${OUTPUT_DIR}/output/voxel"
)

foreach(path IN LISTS forbidden_paths)
    if(EXISTS "${path}")
        message(FATAL_ERROR "Forbidden fallback artifact exists in hero-only review sample: ${path}")
    endif()
endforeach()

file(READ "${OUTPUT_DIR}/OPEN_THIS_FIRST.txt" open_this_first)
if(NOT open_this_first MATCHES "make3d_hero_character_vertex_color.gltf")
    message(FATAL_ERROR "OPEN_THIS_FIRST.txt does not point to the hero semantic vertex-color glTF")
endif()

file(READ "${OUTPUT_DIR}/PORTFOLIO_EVIDENCE.md" evidence)
if(NOT evidence MATCHES "hero-only review mode")
    message(FATAL_ERROR "PORTFOLIO_EVIDENCE.md does not explain hero-only review mode")
endif()
if(NOT evidence MATCHES "semantic vertex-color glTF")
    message(FATAL_ERROR "PORTFOLIO_EVIDENCE.md does not describe semantic vertex-color glTF")
endif()

file(READ "${OUTPUT_DIR}/output/hero/make3d_hero_character_vertex_color.gltf" hero_gltf)
if(NOT hero_gltf MATCHES "COLOR_0")
    message(FATAL_ERROR "Hero semantic glTF does not contain COLOR_0")
endif()

file(READ "${OUTPUT_DIR}/output/game_asset/make3d_game_asset_manifest.json" game_asset_manifest)
if(NOT game_asset_manifest MATCHES "classification")
    message(FATAL_ERROR "Production game asset manifest does not include classification")
endif()

file(READ "${OUTPUT_DIR}/output/production_report.json" production_json)
if(NOT production_json MATCHES "gameAssetObj")
    message(FATAL_ERROR "Production report JSON does not include gameAssetObj")
endif()
if(NOT production_json MATCHES "gameAssetManifest")
    message(FATAL_ERROR "Production report JSON does not include gameAssetManifest")
endif()

message(STATUS "Production sample artifact contract verified: ${OUTPUT_DIR}")
