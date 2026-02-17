#include "ui/ui_legends.hpp"
#include <algorithm>
#include <cstdint>

void drawLegendBar(ImDrawList* dl, ImVec2 pos, float width, float height,
                   ImU32 (*colorFn)(float t, float param), float param,
                   const char* labelLeft, const char* labelRight) {
  const int nSegs = 64;
  float segW = width / (float)nSegs;
  for (int i = 0; i < nSegs; i++) {
    float t0 = (float)i / (float)nSegs;
    float t1 = (float)(i + 1) / (float)nSegs;
    ImU32 c0 = colorFn(t0, param);
    ImU32 c1 = colorFn(t1, param);
    ImVec2 p0(pos.x + i * segW, pos.y);
    ImVec2 p1(pos.x + (i + 1) * segW, pos.y + height);
    dl->AddRectFilledMultiColor(p0, p1, c0, c1, c1, c0);
  }
  dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(60, 60, 60, 255));
  if (labelLeft)
    dl->AddText(ImVec2(pos.x, pos.y + height + 2), IM_COL32(100, 100, 100, 255), labelLeft);
  if (labelRight) {
    ImVec2 sz = ImGui::CalcTextSize(labelRight);
    dl->AddText(ImVec2(pos.x + width - sz.x, pos.y + height + 2), IM_COL32(100, 100, 100, 255), labelRight);
  }
}

ImU32 trigramLegendColor(float t, float brightness) {
  float colorBeginR = 1.0f, colorBeginG = 0.3f, colorBeginB = 0.1f;
  float colorEndR = 0.1f, colorEndG = 0.5f, colorEndB = 1.0f;
  float r = colorBeginR + (colorEndR - colorBeginR) * t;
  float gn = colorBeginG + (colorEndG - colorBeginG) * t;
  float b = colorBeginB + (colorEndB - colorBeginB) * t;
  float bScale = brightness / 100.0f;
  bScale = bScale * bScale * bScale;
  float saturation = bScale * 2.0f;
  r = std::min(1.0f, r * saturation);
  gn = std::min(1.0f, gn * saturation);
  b = std::min(1.0f, b * saturation);
  return IM_COL32((uint8_t)(r * 255), (uint8_t)(gn * 255), (uint8_t)(b * 255), 255);
}

ImU32 bigramLegendColor(float t, float /*unused*/) {
  float r = (1.0f - t);
  float gn = 0.5f;
  float b = t;
  return IM_COL32((uint8_t)(r * 255), (uint8_t)(gn * 200), (uint8_t)(b * 255), 255);
}

ImU32 bigramFireLegendColor(float t, float /*unused*/) {
  if (t <= 0.0f) return IM_COL32(0, 0, 0, 255);
  if (t >= 1.0f) return IM_COL32(255, 255, 255, 255);
  if (t < 0.2f) return IM_COL32(0, 0, (int)(102 * t * 5.f), 255);
  if (t < 0.4f) {
    float u = (t - 0.2f) * 5.f;
    return IM_COL32(0, (int)(153 * u), (int)(102 + 127 * u), 255);
  }
  if (t < 0.6f) {
    float u = (t - 0.4f) * 5.f;
    return IM_COL32(0, (int)(153 + 77 * u), (int)(229 - 51 * u), 255);
  }
  if (t < 0.8f) {
    float u = (t - 0.6f) * 5.f;
    return IM_COL32((int)(217 * u), (int)(230 - 38 * u), (int)(178 - 178 * u), 255);
  }
  {
    float u = (t - 0.8f) * 5.f;
    return IM_COL32(217, (int)(192 + 63 * u), (int)(217 * u), 255);
  }
}

ImU32 entropyLegendColor(float t, float /*unused*/) {
  uint8_t r, gn, b;
  if (t < 0.5f) {
    r = (uint8_t)(t * 2.0f * 200);
    gn = (uint8_t)(80 + t * 2.0f * 80);
    b = (uint8_t)(60 * (1.0f - t * 2.0f));
  } else {
    float s = (t - 0.5f) * 2.0f;
    r = (uint8_t)(200 + s * 55);
    gn = (uint8_t)(160 * (1.0f - s) + 60 * s);
    b = (uint8_t)(20 * s);
  }
  return IM_COL32(r, gn, b, 255);
}

ImU32 histogramLegendColor(float t, float /*unused*/) {
  uint8_t r = (uint8_t)(40 + 30 * t);
  uint8_t gn = (uint8_t)(60 + 100 * t);
  uint8_t b = (uint8_t)(130 * (1.0f - t) + 40 * t);
  return IM_COL32(r, gn, b, 230);
}
