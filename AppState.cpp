#include "AppState.h"
#include <fstream>

bool SaveProject(const char* path, const GUIState& state) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    int partCount = (int)state.partViewImages.size();
    ofs.write((char*)&partCount, sizeof(int));
    for (const auto& part : state.partViewImages) {
        int p = (int)part.first;
        ofs.write((char*)&p, sizeof(int));
        int viewCount = (int)part.second.size();
        ofs.write((char*)&viewCount, sizeof(int));
        for (const auto& view : part.second) {
            int v = (int)view.first;
            ofs.write((char*)&v, sizeof(int));
            int len = (int)view.second.size();
            ofs.write((char*)&len, sizeof(int));
            ofs.write(view.second.c_str(), len);
        }
    }
    ofs.write((char*)&state.polygonCount, sizeof(int));
    ofs.write((char*)&state.exportCombined, sizeof(bool));
    int su = (int)state.scaleUnit;
    ofs.write((char*)&su, sizeof(int));
    return true;
}

bool LoadProject(const char* path, GUIState& state) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    int partCount = 0;
    ifs.read((char*)&partCount, sizeof(int));
    for (int i = 0; i < partCount; ++i) {
        int p = 0, viewCount = 0;
        ifs.read((char*)&p, sizeof(int));
        ifs.read((char*)&viewCount, sizeof(int));
        for (int j = 0; j < viewCount; ++j) {
            int v = 0, len = 0;
            ifs.read((char*)&v, sizeof(int));
            ifs.read((char*)&len, sizeof(int));
            std::string path(len, '\0');
            ifs.read(&path[0], len);
            state.partViewImages[(PartType)p][(ViewType)v] = path;
        }
    }
    ifs.read((char*)&state.polygonCount, sizeof(int));
    ifs.read((char*)&state.exportCombined, sizeof(bool));
    int su = 0;
    ifs.read((char*)&su, sizeof(int));
    state.scaleUnit = (ExportScaleUnit)su;
    return true;
}
