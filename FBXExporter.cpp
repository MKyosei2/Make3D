#include "FBXExporter.h"
#include <fstream>
void ExportToFBX(const Mesh3D& mesh, const char* filename) {
    std::ofstream ofs(filename);
    ofs << "FBX dummy export: vertices = " << mesh.vertices.size() << "\n";
    ofs.close();
}
