#pragma once

#include "common.h"

// ボクセルからメッシュを構築する
Mesh3D BuildMeshFromVolume(const Volume& vol);

// ポリゴン数を間引いて縮小する
Mesh3D ReduceMesh(const Mesh3D& mesh, int targetTriangles);