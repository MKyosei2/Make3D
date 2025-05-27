#pragma once
enum class PartType { Head, Arm, Leg, Body };
PartType ClassifyPart(int x, int y, int width, int height, int imageW, int imageH);
