#pragma once
#include <string>
#include "MeshUtils.h"

// FBX形式でメッシュを書き出す
bool exportMeshToFBX(const std::wstring& filename, const Mesh& mesh);
