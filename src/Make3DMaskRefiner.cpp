#include "Make3DMaskRefiner.h"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <sstream>

namespace make3d {

namespace {

static int CountForeground(const std::vector<std::uint8_t>& mask) {
    int count = 0;
    for (std::uint8_t v : mask) if (v) ++count;
    return count;
}

static bool Inside(int x, int y, int w, int h) {
    return x >= 0 && y >= 0 && x < w && y < h;
}

static std::vector<std::vector<int>> Components(const std::vector<std::uint8_t>& mask, int w, int h) {
    std::vector<int> visited(static_cast<size_t>(w) * h, 0);
    std::vector<std::vector<int>> comps;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int start = y * w + x;
            if (!mask[static_cast<size_t>(start)] || visited[static_cast<size_t>(start)]) continue;
            std::vector<int> comp;
            std::queue<int> q;
            q.push(start);
            visited[static_cast<size_t>(start)] = 1;
            while (!q.empty()) {
                int p = q.front();
                q.pop();
                comp.push_back(p);
                int px = p % w;
                int py = p / w;
                for (int k = 0; k < 4; ++k) {
                    int nx = px + dx[k];
                    int ny = py + dy[k];
                    if (!Inside(nx, ny, w, h)) continue;
                    int ni = ny * w + nx;
                    if (mask[static_cast<size_t>(ni)] && !visited[static_cast<size_t>(ni)]) {
                        visited[static_cast<size_t>(ni)] = 1;
                        q.push(ni);
                    }
                }
            }
            comps.push_back(std::move(comp));
        }
    }
    return comps;
}

static int KeepLargeComponents(std::vector<std::uint8_t>& mask, int w, int h, const MaskRefineOptions& options) {
    auto comps = Components(mask, w, h);
    if (comps.empty()) return 0;
    size_t largest = 0;
    for (size_t i = 1; i < comps.size(); ++i) if (comps[i].size() > comps[largest].size()) largest = i;
    std::vector<std::uint8_t> out(mask.size(), 0);
    int removed = 0;
    for (size_t i = 0; i < comps.size(); ++i) {
        bool keep = options.keepLargestComponentOnly ? (i == largest) : (static_cast<int>(comps[i].size()) >= options.minComponentPixels);
        if (keep) {
            for (int p : comps[i]) out[static_cast<size_t>(p)] = 1;
        } else {
            ++removed;
        }
    }
    mask.swap(out);
    return removed;
}

static int FillHoles(std::vector<std::uint8_t>& mask, int w, int h) {
    std::vector<std::uint8_t> outside(mask.size(), 0);
    std::queue<int> q;
    auto push = [&](int x, int y) {
        if (!Inside(x, y, w, h)) return;
        int p = y * w + x;
        if (mask[static_cast<size_t>(p)] || outside[static_cast<size_t>(p)]) return;
        outside[static_cast<size_t>(p)] = 1;
        q.push(p);
    };
    for (int x = 0; x < w; ++x) { push(x, 0); push(x, h - 1); }
    for (int y = 0; y < h; ++y) { push(0, y); push(w - 1, y); }
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        int p = q.front();
        q.pop();
        int x = p % w;
        int y = p / w;
        for (int k = 0; k < 4; ++k) push(x + dx[k], y + dy[k]);
    }
    int filled = 0;
    for (size_t i = 0; i < mask.size(); ++i) {
        if (!mask[i] && !outside[i]) {
            mask[i] = 1;
            ++filled;
        }
    }
    return filled;
}

static void SmoothMask(std::vector<std::uint8_t>& mask, int w, int h, int iterations) {
    std::vector<std::uint8_t> next = mask;
    for (int it = 0; it < iterations; ++it) {
        next = mask;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int count = 0;
                for (int yy = -1; yy <= 1; ++yy) {
                    for (int xx = -1; xx <= 1; ++xx) {
                        if (mask[static_cast<size_t>((y + yy) * w + (x + xx))]) ++count;
                    }
                }
                next[static_cast<size_t>(y * w + x)] = count >= 5 ? 1 : 0;
            }
        }
        mask.swap(next);
    }
}

static std::string JsonEscape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else o << c;
    }
    return o.str();
}

} // namespace

MaskRefineReport RefineForegroundMask(std::vector<std::uint8_t>& mask, int width, int height, const MaskRefineOptions& options) {
    MaskRefineReport report;
    report.width = width;
    report.height = height;
    report.foregroundBefore = CountForeground(mask);
    report.componentsBefore = static_cast<int>(Components(mask, width, height).size());

    report.removedComponents = KeepLargeComponents(mask, width, height, options);
    if (options.fillInteriorHoles) report.filledHolePixels = FillHoles(mask, width, height);
    if (options.smoothJaggedEdges) {
        SmoothMask(mask, width, height, options.smoothingIterations);
        report.smoothingIterations = options.smoothingIterations;
    }
    report.componentsAfter = static_cast<int>(Components(mask, width, height).size());
    report.foregroundAfter = CountForeground(mask);
    return report;
}

std::string MaskRefineReport::ToMarkdown() const {
    std::ostringstream o;
    o << "# Mask Refinement Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Width | " << width << " |\n";
    o << "| Height | " << height << " |\n";
    o << "| Foreground before | " << foregroundBefore << " |\n";
    o << "| Foreground after | " << foregroundAfter << " |\n";
    o << "| Components before | " << componentsBefore << " |\n";
    o << "| Components after | " << componentsAfter << " |\n";
    o << "| Removed components | " << removedComponents << " |\n";
    o << "| Filled hole pixels | " << filledHolePixels << " |\n";
    o << "| Smoothing iterations | " << smoothingIterations << " |\n";
    return o.str();
}

std::string MaskRefineReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"width\": " << width << ",\n";
    o << "  \"height\": " << height << ",\n";
    o << "  \"foregroundBefore\": " << foregroundBefore << ",\n";
    o << "  \"foregroundAfter\": " << foregroundAfter << ",\n";
    o << "  \"componentsBefore\": " << componentsBefore << ",\n";
    o << "  \"componentsAfter\": " << componentsAfter << ",\n";
    o << "  \"removedComponents\": " << removedComponents << ",\n";
    o << "  \"filledHolePixels\": " << filledHolePixels << ",\n";
    o << "  \"smoothingIterations\": " << smoothingIterations << "\n";
    o << "}";
    return o.str();
}

} // namespace make3d
