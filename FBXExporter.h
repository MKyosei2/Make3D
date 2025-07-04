#pragma once
#include <string>
#include "MeshGenerator.h"

bool exportMeshToFBX(const Mesh& mesh, const std::wstring& filename);
void ExportMeshToFBX(const std::vector<Triangle>& triangles, const std::vector<Vertex>& vertices, const std::string& filename);
