#include "viz/range_selector.hpp"
#include "core/app_state.hpp"
#include "imgui.h"
#include <cstddef>
#include <functional>
#include <algorithm>

namespace {

const float kSelectorWidthPercent = 0.15f;  // both strips together = 15% of parent
const float kThumbH = 8.0f;                  // thumb height (along the range axis)
const ImU32 kSelColor = IM_COL32(0x9C, 0x5B, 0xE8, 255);
const ImU32 kSelFill = IM_COL32(0x9C, 0x5B, 0xE8, 70);

// Vertical strip: top = range start, bottom = range end. One pixel per row.
void drawChunkStripVertical(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max,
                            const uint8_t* bytes, size_t start, size_t end) {
  size_t sz = end - start;
  if (sz == 0) return;
  float stripH = p_max.y - p_min.y;
  float stripW = p_max.x - p_min.x;
  for (float y = 0; y < stripH; y += 1.0f) {
    size_t i = start + (size_t)((y / stripH) * sz) % sz;
    uint8_t v = bytes[i];
    float gray = (float)v / 255.0f;
    ImU32 col = IM_COL32((int)(gray * 255), (int)(gray * 255), (int)(gray * 255), 255);
    dl->AddLine(ImVec2(p_min.x, p_min.y + y), ImVec2(p_max.x, p_min.y + y), col);
  }
}

// One vertical strip: byte range along Y (top=rangeStart, bottom=rangeEnd).
// Selection [selStart, selEnd] maps to topY..bottomY. Thumbs at top/bottom; body drag moves selection.
void drawOneStripVertical(ImDrawList* dl, const uint8_t* bytes,
                          size_t rangeStart, size_t rangeEnd,
                          size_t selStart, size_t selEnd,
                          size_t* outSelStart, size_t* outSelEnd,
                          ImVec2 stripMin, float stripW, float stripH,
                          const char* idPrefix, std::function<void()> onChanged) {
  size_t rangeLen = rangeEnd - rangeStart;
  if (rangeLen == 0) return;

  ImVec2 stripMax(stripMin.x + stripW, stripMin.y + stripH);

  drawChunkStripVertical(dl, stripMin, stripMax, bytes, rangeStart, rangeEnd);

  float startF = (float)(selStart - rangeStart) / (float)rangeLen;
  float endF = (float)(selEnd - rangeStart) / (float)rangeLen;
  startF = std::max(0.0f, std::min(1.0f, startF));
  endF = std::max(0.0f, std::min(1.0f, endF));
  if (startF > endF) endF = startF;

  float topY = stripMin.y + startF * stripH;
  float bottomY = stripMin.y + endF * stripH;
  if (bottomY < topY + kThumbH) bottomY = topY + kThumbH;

  dl->AddRectFilled(ImVec2(stripMin.x, topY), ImVec2(stripMax.x, bottomY), kSelFill);

  float topThumbMin = topY - kThumbH * 0.5f;
  float topThumbMax = topY + kThumbH * 0.5f;
  float bottomThumbMin = bottomY - kThumbH * 0.5f;
  float bottomThumbMax = bottomY + kThumbH * 0.5f;

  dl->AddRectFilled(ImVec2(stripMin.x, topThumbMin), ImVec2(stripMax.x, topThumbMax), kSelColor);
  dl->AddRect(ImVec2(stripMin.x, topThumbMin), ImVec2(stripMax.x, topThumbMax), IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.0f);
  dl->AddRectFilled(ImVec2(stripMin.x, bottomThumbMin), ImVec2(stripMax.x, bottomThumbMax), kSelColor);
  dl->AddRect(ImVec2(stripMin.x, bottomThumbMin), ImVec2(stripMax.x, bottomThumbMax), IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.0f);

  auto fileFromY = [&](float my) {
    float t = (my - stripMin.y) / stripH;
    t = std::max(0.0f, std::min(1.0f, t));
    return rangeStart + (size_t)(t * (float)rangeLen);
  };

  ImGui::PushID(idPrefix);
  ImGui::SetCursorScreenPos(ImVec2(stripMin.x, topThumbMin));
  ImGui::InvisibleButton("##thumb_start", ImVec2(stripW, kThumbH));
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    size_t newStart = fileFromY(ImGui::GetIO().MousePos.y);
    if (newStart >= rangeEnd) newStart = rangeEnd > rangeStart ? rangeEnd - 1 : rangeStart;
    *outSelStart = newStart;
    if (*outSelEnd <= *outSelStart) *outSelEnd = (*outSelStart + 1 < rangeEnd) ? *outSelStart + 1 : rangeEnd;
    *outSelEnd = std::min(*outSelEnd, rangeEnd);
    onChanged();
  }

  ImGui::SetCursorScreenPos(ImVec2(stripMin.x, bottomThumbMin));
  ImGui::InvisibleButton("##thumb_end", ImVec2(stripW, kThumbH));
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    size_t newEnd = fileFromY(ImGui::GetIO().MousePos.y);
    if (newEnd <= rangeStart) newEnd = rangeStart + 1;
    newEnd = std::min(newEnd, rangeEnd);
    *outSelEnd = newEnd;
    if (*outSelStart >= *outSelEnd) *outSelStart = *outSelEnd > rangeStart ? *outSelEnd - 1 : rangeStart;
    onChanged();
  }

  ImGui::SetCursorScreenPos(ImVec2(stripMin.x, topThumbMax));
  float bodyH = std::max(8.0f, bottomThumbMin - topThumbMax);  // min 8px so body is easy to grab
  ImGui::InvisibleButton("##body", ImVec2(stripW, bodyH));
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    float dy = ImGui::GetIO().MouseDelta.y;
    if (dy != 0.0f) {
      float t = dy / stripH;
      long delta = (long)(t * (float)rangeLen);  // signed so drag-up moves selection up
      if (delta != 0) {
        size_t len = *outSelEnd - *outSelStart;
        long newStartL = (long)*outSelStart + delta;
        if (newStartL < (long)rangeStart) newStartL = (long)rangeStart;
        if (newStartL > (long)(rangeEnd - len)) newStartL = (long)(rangeEnd - len);
        size_t newStart = (size_t)newStartL;
        *outSelStart = newStart;
        *outSelEnd = newStart + len;
        onChanged();
      }
    }
  }
  ImGui::PopID();

  ImGui::SetCursorScreenPos(ImVec2(stripMin.x, stripMax.y));
}

}  // namespace

float getVizRangeSelectorWidth(float parentContentWidth) {
  return kSelectorWidthPercent * parentContentWidth;
}

void drawVizRangeSelector(
    const uint8_t* bytes,
    size_t fileSz,
    float stripWidthPx,
    std::function<void()> onRangeChanged) {
  if (!bytes || fileSz == 0) return;
  if (stripWidthPx < 4.0f) stripWidthPx = 4.0f;

  bool wholeFocus = (g.focusEnd == 0);
  size_t focusStart = wholeFocus ? 0 : g.focusStart;
  size_t focusEnd = wholeFocus ? fileSz : g.focusEnd;
  if (focusEnd <= focusStart) focusEnd = focusStart + 1;
  if (focusEnd > fileSz) focusEnd = fileSz;

  bool wholeViz = (g.vizRangeEnd == 0);
  size_t vizStart = wholeViz ? focusStart : std::max(g.vizRangeStart, focusStart);
  size_t vizEnd = wholeViz ? focusEnd : std::min(g.vizRangeEnd, focusEnd);
  if (vizEnd <= vizStart) vizEnd = vizStart + 1;
  if (vizEnd > focusEnd) vizEnd = focusEnd;
  if (vizStart < focusStart) vizStart = focusStart;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  // Strip 1 (All data) — own dock with border
  ImGui::BeginChild("VizStrip1", ImVec2(stripWidthPx, 0),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
  float stripH = ImGui::GetContentRegionAvail().y;
  if (stripH < 20.0f) stripH = 20.0f;
  ImVec2 s1Min = ImGui::GetCursorScreenPos();
  ImDrawList* dl1 = ImGui::GetWindowDrawList();
  size_t newFocusStart = focusStart;
  size_t newFocusEnd = focusEnd;
  drawOneStripVertical(dl1, bytes, 0, fileSz, focusStart, focusEnd,
                       &newFocusStart, &newFocusEnd, s1Min, stripWidthPx, stripH, "strip1",
                       [&] {
                         if (newFocusStart == 0 && newFocusEnd >= fileSz) {
                           g.focusStart = 0;
                           g.focusEnd = 0;
                           g.vizRangeStart = 0;
                           g.vizRangeEnd = 0;
                         } else {
                           g.focusStart = newFocusStart;
                           g.focusEnd = std::min(newFocusEnd, fileSz);
                           if (g.focusEnd <= g.focusStart) g.focusEnd = g.focusStart + 1;
                           g.vizRangeStart = g.focusStart;
                           g.vizRangeEnd = g.focusEnd;
                         }
                         if (onRangeChanged) onRangeChanged();
                       });
  if (newFocusStart == 0 && newFocusEnd >= fileSz) {
    g.focusStart = 0;
    g.focusEnd = 0;
  } else {
    g.focusStart = newFocusStart;
    g.focusEnd = std::min(newFocusEnd, fileSz);
    if (g.focusEnd <= g.focusStart) g.focusEnd = g.focusStart + 1;
  }
  ImGui::EndChild();

  // Strip 2 only shows and selects within what strip 1 (focus) selected.
  bool wholeFocusNow = (g.focusEnd == 0);
  size_t focusStart2 = wholeFocusNow ? 0 : g.focusStart;
  size_t focusEnd2 = wholeFocusNow ? fileSz : g.focusEnd;
  if (focusEnd2 <= focusStart2) focusEnd2 = focusStart2 + 1;
  if (focusEnd2 > fileSz) focusEnd2 = fileSz;

  size_t vizStart2 = (g.vizRangeEnd == 0) ? focusStart2 : std::max(g.vizRangeStart, focusStart2);
  size_t vizEnd2 = (g.vizRangeEnd == 0) ? focusEnd2 : std::min(g.vizRangeEnd, focusEnd2);
  if (vizEnd2 <= vizStart2) vizEnd2 = vizStart2 + 1;
  vizStart2 = std::min(vizStart2, focusEnd2 - 1);
  vizEnd2 = std::min(vizEnd2, focusEnd2);

  ImGui::SameLine(0, 0);

  // Strip 2 (Focus) — only the range from strip 1; selection clamped to that
  ImGui::BeginChild("VizStrip2", ImVec2(stripWidthPx, 0),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
  stripH = ImGui::GetContentRegionAvail().y;
  if (stripH < 20.0f) stripH = 20.0f;
  ImVec2 s2Min = ImGui::GetCursorScreenPos();
  ImDrawList* dl2 = ImGui::GetWindowDrawList();
  size_t newVizStart = vizStart2;
  size_t newVizEnd = vizEnd2;
  drawOneStripVertical(dl2, bytes, focusStart2, focusEnd2, vizStart2, vizEnd2,
                       &newVizStart, &newVizEnd, s2Min, stripWidthPx, stripH, "strip2",
                       [&] {
                         size_t vStart = std::max(newVizStart, focusStart2);
                         size_t vEnd = std::min(newVizEnd, focusEnd2);
                         if (vEnd <= vStart) vEnd = vStart + 1;
                         g.vizRangeStart = vStart;
                         g.vizRangeEnd = vEnd;
                         if (onRangeChanged) onRangeChanged();
                       });
  g.vizRangeStart = std::max(newVizStart, focusStart2);
  g.vizRangeEnd = std::min(newVizEnd, focusEnd2);
  if (g.vizRangeEnd <= g.vizRangeStart) g.vizRangeEnd = g.vizRangeStart + 1;
  ImGui::EndChild();

  ImGui::PopStyleVar();
}
