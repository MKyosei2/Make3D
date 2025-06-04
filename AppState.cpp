#include "AppState.h"
#include "BuildVolumeFromImages.h"
#include "ImageUtils.h"
#include "MeshGenerator.h"
#include "PartProcessor.h"
#include "FBXExporter.h"
#include "GUIState.h"

#include <iostream>
#include <map>

extern GUIState guiState;

AppState::AppState() {
    volumeData = nullptr;
    meshData = nullptr;
}

AppState::~AppState() {
    delete volumeData;
    delete meshData;
}

void AppState::generateVolumeFromImages() {
    if (!guiState.viewImages.empty()) {
        std::map<ViewDirection, ImageData> inputImages;
        for (const auto& [dir, imgData] : guiState.viewImages) {
            if (imgData.pixels && imgData.width > 0 && imgData.height > 0) {
                inputImages[dir] = imgData;
            }
        }

        if (!inputImages.empty()) {
            delete volumeData;
            volumeData = BuildVolumeFromImages::buildVolumeFromMultipleImages(inputImages);
        }
    }
}

void AppState::processParts() {
    if (!volumeData) return;

    PartProcessor processor;
    processor.setSelectionRegions(guiState.partSelectionRegions);
    volumeData = processor.process(volumeData);
}

void AppState::generateMesh() {
    if (!volumeData) return;

    MeshGenerator generator;
    generator.setTargetPolygonCount(guiState.polygonCount);
    delete meshData;
    meshData = generator.generate(volumeData);

    // プレビュー用にも共有
    guiState.previewMesh = meshData;
    guiState.enablePreview = true;
}

void AppState::exportToFBX(const std::string& filename) {
    if (!meshData) return;

    FBXExporter exporter;
    exporter.exportMeshToFBX(*meshData, filename);
}
