#pragma once
#include <string>
#include <vector>

class VolumeBuilder
{
public:
    VolumeBuilder();

    // 画像一枚からボリュームデータを構築する
    bool BuildFromImage(const std::wstring& imagePath, int width, int height, int depth);

    // ボリュームデータへのアクセス
    const std::vector<unsigned char>& GetVolumeData() const;

private:
    std::vector<unsigned char> volumeData;
};
