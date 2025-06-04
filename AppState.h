#pragma once
#include <string> 
#include "common.h"
#include "MeshUtils.h"

// アプリケーション全体の状態を保持・処理するクラス
class AppState {
public:
    AppState();
    ~AppState();

    void generateVolumeFromImages();
    void processParts();
    void generateMesh();
    void exportToFBX(const std::string& filename);

private:
    VolumeData* volumeData;
    MeshData* meshData;
};
