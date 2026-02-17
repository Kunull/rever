#pragma once

#include <cstddef>
#include <functional>

// Binglide-style side panel: chunk selectors to choose which byte range is used for
// visualization. Reusable by Trigram, Bigram, or other viz tabs.
// Reads/writes AppState::vizRangeStart, vizRangeEnd (0 = whole file).
// onRangeChanged() is called when the user selects "All" or a chunk.

// stripWidthPx = width of one strip (7.5% of parent content width). Two strips = 15% total.
void drawVizRangeSelector(
  const uint8_t* bytes,
  size_t fileSz,
  float stripWidthPx,
  std::function<void()> onRangeChanged
);

// Total width for both strips (15% of parent). Call with parent content width.
float getVizRangeSelectorWidth(float parentContentWidth);
