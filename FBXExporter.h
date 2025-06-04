#pragma once
#include <fbxsdk.h>

#include "MeshUtils.h"
#include <string>

class FBXExporter {
public:
    FBXExporter();
    ~FBXExporter();

    void exportMeshToFBX(const MeshData& mesh, const std::string& filename);

private:
    fbxsdk::FbxManager* manager = nullptr;
    fbxsdk::FbxIOSettings* ios = nullptr;
};