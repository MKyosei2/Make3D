#pragma once

#include "VolumeUtils.h"
#include "GUIState.h"
#include <map>

namespace BuildVolumeFromImages {

	// 複数視点画像を使って3Dボリュームを構築する
	VolumeData* buildVolumeFromMultipleImages(const std::map<ViewDirection, ImageData>& images);

} // namespace BuildVolumeFromImages
