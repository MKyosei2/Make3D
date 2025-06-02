#pragma once

#include <vector>
#include <string>
#include "common.h"

// メッシュをFBX形式で出力する
bool ExportToFBX(const std::string& path, const std::vector<Mesh3D>& meshes);