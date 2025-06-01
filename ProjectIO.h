#pragma once
#include "GUIState.h"

bool SaveProject(const char* path, const GUIState& state);
bool LoadProject(const char* path, GUIState& state);
