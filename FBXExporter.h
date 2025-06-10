#pragma once
#include <string>
#include "MeshUtils.h"

bool exportMeshToFBX(const std::wstring& filename, const Mesh& mesh);
void ExportMeshToFBX(const std::vector<Triangle>& triangles, const std::string& filename);