#pragma once
#include <string>
#include "MeshUtils.h"

bool exportMeshToFBX(const Mesh& mesh, const std::wstring& filename);
void ExportMeshToFBX(const std::vector<Triangle>& triangles, const std::string& filename);