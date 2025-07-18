#pragma once
#include <string>
#include <vector>

class FBXExporter
{
public:
    bool Export(const std::wstring& filename,
        const std::vector<float>& vertices,  // フラットな x,y,z,... 配列
        const std::vector<unsigned int>& indices);
};
