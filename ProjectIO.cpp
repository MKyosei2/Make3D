#include "ProjectIO.h"
#include <fstream>

bool SaveProject(const char* path, const GUIState& state) {
    std::ofstream ofs(path);
    if (!ofs) return false;

    ofs << "PolygonCount " << state.polygonCount << "\n";
    ofs << "ExportCombined " << (state.exportCombined ? 1 : 0) << "\n";

    for (const auto& part : state.imagePaths) {
        for (const auto& view : part.second) {
            ofs << "ImagePath "
                << (int)part.first << " "
                << (int)view.first << " "
                << "\"" << view.second << "\"\n";
        }
    }

    return true;
}

bool LoadProject(const char* path, GUIState& state) {
    std::ifstream ifs(path);
    if (!ifs) return false;

    state.imagePaths.clear();

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find("PolygonCount") == 0) {
            state.polygonCount = atoi(line.substr(13).c_str());
        }
        else if (line.find("ExportCombined") == 0) {
            state.exportCombined = atoi(line.substr(15).c_str()) != 0;
        }
        else if (line.find("ImagePath") == 0) {
            int part, view;
            char path[512];
            sscanf_s(line.c_str(), "ImagePath %d %d \"%[^\"]", &part, &view, path, (unsigned)_countof(path));
            state.imagePaths[(PartType)part][(ViewType)view] = path;
        }
    }

    return true;
}
