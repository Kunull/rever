#pragma once
#include "imgui.h"

void drawLegendBar(ImDrawList* dl, ImVec2 pos, float width, float height,
                   ImU32 (*colorFn)(float t, float param), float param,
                   const char* labelLeft, const char* labelRight);

ImU32 trigramLegendColor(float t, float brightness);
ImU32 bigramLegendColor(float t, float unused);
ImU32 bigramBlackwall2077LegendColor(float t, float unused);
ImU32 bigramFireLegendColor(float t, float unused);
ImU32 histogramLegendColor(float t, float unused);
ImU32 entropyLegendColor(float t, float unused);
