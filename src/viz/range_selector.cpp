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

  // Strip 1 slide down: toward end of file; stop when focusEnd >= fileSz
  if (g.focusSlidingDown && fileSz > 0) {
    float dt = ImGui::GetIO().DeltaTime;
    size_t windowLen = (g.focusEnd > g.focusStart) ? (g.focusEnd - g.focusStart) : (fileSz / 4);
    if (windowLen == 0) windowLen = 1;
    size_t curStart = (g.focusEnd == 0) ? 0 : g.focusStart;
    size_t curEnd = (g.focusEnd == 0) ? fileSz : g.focusEnd;
    if (curEnd <= curStart) curEnd = curStart + windowLen;
    if (curEnd > fileSz) curEnd = fileSz;
    long step = (long)(g.vizRangePlaySpeed * dt);
    if (step > 0) {
      size_t newStart = curStart + (size_t)step;
      size_t newEnd = newStart + windowLen;
      if (newEnd >= fileSz) {
        newEnd = fileSz;
        newStart = (newEnd >= windowLen) ? (newEnd - windowLen) : 0;
        g.focusSlidingDown = false;
      }
      g.focusStart = newStart;
      g.focusEnd = newEnd;
      g.vizRangeStart = g.focusStart;
      g.vizRangeEnd = g.focusEnd;
      if (onRangeChanged) onRangeChanged();
    }
  }
  // Strip 1 slide up: toward start; stop when focusStart <= 0
  if (g.focusSlidingUp && fileSz > 0) {
    float dt = ImGui::GetIO().DeltaTime;
    size_t windowLen = (g.focusEnd > g.focusStart) ? (g.focusEnd - g.focusStart) : (fileSz / 4);
    if (windowLen == 0) windowLen = 1;
    size_t curStart = (g.focusEnd == 0) ? 0 : g.focusStart;
    size_t curEnd = (g.focusEnd == 0) ? fileSz : g.focusEnd;
    if (curEnd <= curStart) curEnd = curStart + windowLen;
    long step = (long)(g.vizRangePlaySpeed * dt);
    if (step > 0) {
      long newStartL = (long)curStart - step;
      if (newStartL <= 0) {
        g.focusStart = 0;
        g.focusEnd = (windowLen < fileSz) ? windowLen : fileSz;
        g.vizRangeStart = g.focusStart;
        g.vizRangeEnd = g.focusEnd;
        g.focusSlidingUp = false;
      } else {
        g.focusStart = (size_t)newStartL;
        g.focusEnd = g.focusStart + windowLen;
        g.vizRangeStart = g.focusStart;
        g.vizRangeEnd = g.focusEnd;
      }
      if (onRangeChanged) onRangeChanged();
    }
  }

  // Strip 2 slide down: toward end of focus; stop when vizEnd >= focusEnd
  if (g.vizSlidingDown && focusEnd > focusStart) {
    float dt = ImGui::GetIO().DeltaTime;
    size_t windowLen = (g.vizRangeEnd > g.vizRangeStart)
        ? (g.vizRangeEnd - g.vizRangeStart)
        : (focusEnd - focusStart);
    if (windowLen == 0) windowLen = 1;
    size_t curStart = wholeViz ? focusStart : std::max(g.vizRangeStart, focusStart);
    size_t curEnd = wholeViz ? focusEnd : std::min(g.vizRangeEnd, focusEnd);
    if (curEnd <= curStart) curEnd = curStart + windowLen;
    long step = (long)(g.vizRangePlaySpeed * dt);
    if (step > 0) {
      size_t newStart = curStart + (size_t)step;
      size_t newEnd = newStart + windowLen;
      if (newEnd >= focusEnd) {
        newEnd = focusEnd;
        newStart = (newEnd >= windowLen) ? (newEnd - windowLen) : focusStart;
        g.vizSlidingDown = false;
      }
      g.vizRangeStart = newStart;
      g.vizRangeEnd = newEnd;
      if (onRangeChanged) onRangeChanged();
    }
  }
  // Strip 2 slide up: toward start of focus; stop when vizStart <= focusStart
  if (g.vizSlidingUp && focusEnd > focusStart) {
    float dt = ImGui::GetIO().DeltaTime;
    size_t windowLen = (g.vizRangeEnd > g.vizRangeStart)
        ? (g.vizRangeEnd - g.vizRangeStart)
        : (focusEnd - focusStart);
    if (windowLen == 0) windowLen = 1;
    size_t curStart = wholeViz ? focusStart : std::max(g.vizRangeStart, focusStart);
    size_t curEnd = wholeViz ? focusEnd : std::min(g.vizRangeEnd, focusEnd);
    if (curEnd <= curStart) curEnd = curStart + windowLen;
    long step = (long)(g.vizRangePlaySpeed * dt);
    if (step > 0) {
      long newStartL = (long)curStart - step;
      if (newStartL <= (long)focusStart) {
        g.vizRangeStart = focusStart;
        g.vizRangeEnd = focusStart + windowLen;
        if (g.vizRangeEnd > focusEnd) g.vizRangeEnd = focusEnd;
        g.vizSlidingUp = false;
      } else {
        g.vizRangeStart = (size_t)newStartL;
        g.vizRangeEnd = g.vizRangeStart + windowLen;
      }
      if (onRangeChanged) onRangeChanged();
    }
  }

  // Recompute from g after sliding so strip drawing uses updated values (and we don't overwrite them later)
  wholeFocus = (g.focusEnd == 0);
  focusStart = wholeFocus ? 0 : g.focusStart;
  focusEnd = wholeFocus ? fileSz : g.focusEnd;
  if (focusEnd <= focusStart) focusEnd = focusStart + 1;
  if (focusEnd > fileSz) focusEnd = fileSz;

  wholeViz = (g.vizRangeEnd == 0);
  vizStart = wholeViz ? focusStart : std::max(g.vizRangeStart, focusStart);
  vizEnd = wholeViz ? focusEnd : std::min(g.vizRangeEnd, focusEnd);
  if (vizEnd <= vizStart) vizEnd = vizStart + 1;
  if (vizEnd > focusEnd) vizEnd = focusEnd;
  if (vizStart < focusStart) vizStart = focusStart;

  float availY = ImGui::GetContentRegionAvail().y;
  float btnRowH = ImGui::GetFrameHeightWithSpacing();
  float stripH = std::max(20.0f, availY - 2.0f * btnRowH);  // one row above, one row below

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  // Semantic colors: Stop = red, Up/Down (start) = green, disabled = grey
  const ImVec4 stopBg(0.55f, 0.2f, 0.2f, 1.0f), stopHov(0.7f, 0.28f, 0.28f, 1.0f), stopAct(0.48f, 0.18f, 0.18f, 1.0f);
  const ImVec4 goBg(0.2f, 0.5f, 0.2f, 1.0f), goHov(0.28f, 0.58f, 0.28f, 1.0f), goAct(0.18f, 0.45f, 0.18f, 1.0f);
  const ImVec4 disBg(0.22f, 0.22f, 0.22f, 1.0f), disHov(0.26f, 0.26f, 0.26f, 1.0f), disAct(0.2f, 0.2f, 0.2f, 1.0f);

  // Up buttons: enabled only when slider does not touch start of region
  bool focusNotAtStart = (g.focusEnd != 0 && g.focusStart > 0);
  bool vizNotAtStart = (g.vizRangeEnd != 0 && g.vizRangeStart > focusStart);
  ImGui::PushID("strip1_up");
  ImGui::BeginDisabled(!focusNotAtStart);
  if (!focusNotAtStart) {
    ImGui::PushStyleColor(ImGuiCol_Button, disBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, disAct);
  } else if (g.focusSlidingUp) {
    ImGui::PushStyleColor(ImGuiCol_Button, stopBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, stopHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, stopAct);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, goBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, goHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, goAct);
  }
  if (ImGui::Button(g.focusSlidingUp ? "Stop" : "Up", ImVec2(stripWidthPx, 0))) {
    g.focusSlidingUp = !g.focusSlidingUp;
    if (onRangeChanged) onRangeChanged();
  }
  ImGui::PopStyleColor(3);
  ImGui::EndDisabled();
  ImGui::PopID();
  ImGui::SameLine(0, 0);
  ImGui::PushID("strip2_up");
  ImGui::BeginDisabled(!vizNotAtStart);
  if (!vizNotAtStart) {
    ImGui::PushStyleColor(ImGuiCol_Button, disBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, disAct);
  } else if (g.vizSlidingUp) {
    ImGui::PushStyleColor(ImGuiCol_Button, stopBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, stopHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, stopAct);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, goBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, goHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, goAct);
  }
  if (ImGui::Button(g.vizSlidingUp ? "Stop" : "Up", ImVec2(stripWidthPx, 0))) {
    g.vizSlidingUp = !g.vizSlidingUp;
    if (onRangeChanged) onRangeChanged();
  }
  ImGui::PopStyleColor(3);
  ImGui::EndDisabled();
  ImGui::PopID();

  // Strip 1 (All data) — own dock with border
  ImGui::BeginChild("VizStrip1", ImVec2(stripWidthPx, stripH),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
  float s1H = ImGui::GetContentRegionAvail().y;
  if (s1H < 20.0f) s1H = 20.0f;
  ImVec2 s1Min = ImGui::GetCursorScreenPos();
  ImDrawList* dl1 = ImGui::GetWindowDrawList();
  size_t newFocusStart = focusStart;
  size_t newFocusEnd = focusEnd;
  drawOneStripVertical(dl1, bytes, 0, fileSz, focusStart, focusEnd,
                       &newFocusStart, &newFocusEnd, s1Min, stripWidthPx, s1H, "strip1",
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
  ImGui::BeginChild("VizStrip2", ImVec2(stripWidthPx, stripH),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
  float s2H = ImGui::GetContentRegionAvail().y;
  if (s2H < 20.0f) s2H = 20.0f;
  ImVec2 s2Min = ImGui::GetCursorScreenPos();
  ImDrawList* dl2 = ImGui::GetWindowDrawList();
  size_t newVizStart = vizStart2;
  size_t newVizEnd = vizEnd2;
  drawOneStripVertical(dl2, bytes, focusStart2, focusEnd2, vizStart2, vizEnd2,
                       &newVizStart, &newVizEnd, s2Min, stripWidthPx, s2H, "strip2",
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

  // Down buttons: Stop = red, Down (start) = green, disabled = grey
  bool focusNotAtEnd = (g.focusEnd != 0 && g.focusEnd < fileSz);
  bool vizNotAtEnd = (g.vizRangeEnd != 0 && g.vizRangeEnd < focusEnd);
  ImGui::PushID("strip1_down");
  ImGui::BeginDisabled(!focusNotAtEnd);
  if (!focusNotAtEnd) {
    ImGui::PushStyleColor(ImGuiCol_Button, disBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, disAct);
  } else if (g.focusSlidingDown) {
    ImGui::PushStyleColor(ImGuiCol_Button, stopBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, stopHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, stopAct);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, goBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, goHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, goAct);
  }
  if (ImGui::Button(g.focusSlidingDown ? "Stop" : "Down", ImVec2(stripWidthPx, 0))) {
    g.focusSlidingDown = !g.focusSlidingDown;
    if (onRangeChanged) onRangeChanged();
  }
  ImGui::PopStyleColor(3);
  ImGui::EndDisabled();
  ImGui::PopID();
  ImGui::SameLine(0, 0);
  ImGui::PushID("strip2_down");
  ImGui::BeginDisabled(!vizNotAtEnd);
  if (!vizNotAtEnd) {
    ImGui::PushStyleColor(ImGuiCol_Button, disBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, disAct);
  } else if (g.vizSlidingDown) {
    ImGui::PushStyleColor(ImGuiCol_Button, stopBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, stopHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, stopAct);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, goBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, goHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, goAct);
  }
  if (ImGui::Button(g.vizSlidingDown ? "Stop" : "Down", ImVec2(stripWidthPx, 0))) {
    g.vizSlidingDown = !g.vizSlidingDown;
    if (onRangeChanged) onRangeChanged();
  }
  ImGui::PopStyleColor(3);
  ImGui::EndDisabled();
  ImGui::PopID();

  ImGui::PopStyleVar();
}
