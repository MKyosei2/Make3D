#include "ProjectIO.h"
#include <fstream>
bool SaveProject(const char* path, const GUIState& state) {
    std::ofstream ofs(path);
    for (auto& part : state.imagePaths) {
        for (auto& view : part.second) {
            ofs << (int)part.first << " " << (int)view.first << " " << view.second << "\n";
        }
    }
    return true;
}

bool LoadProject(const char* path, GUIState& state) {
    std::ifstream ifs(path);
    int p, v; std::string pathStr;
    while (ifs >> p >> v >> pathStr) {
        state.imagePaths[(PartType)p][(ViewType)v] = pathStr;
    }
    return true;
}