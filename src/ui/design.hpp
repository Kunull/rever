#pragma once

// Rever design language: ImHex-inspired layout tokens and helpers.
// Spacing/padding follow ImHex (styles.imgui); theme is AMOLED, no rounding.
#include "imgui.h"

namespace Design {

// ─── Layout tokens (ImHex-style) ───────────────────────────────────────────
// Use these instead of hardcoded SameLine(x) / TableSetupColumn width.

// Form / property rows: label column width (tables and inline label+widget).
const float LabelWidth = 80.0f;

// Toolbar with multiple groups (e.g. Disassembly: Arch | Base | Button).
const float ToolbarCol1 = 60.0f;   // after first short label (e.g. "Arch")
const float ToolbarCol2 = 140.0f;   // after second label (e.g. "Base")

// Inline form rows with two label+widget pairs on one line (e.g. Strings).
const float InlineLabel1Col = 88.0f;   // widget start after first label
const float InlineLabel2Col = 148.0f;  // widget start after second label

// Search panel: Mode combo, then Pattern input.
const float SearchModeCol = 52.0f;
const float SearchPatternCol = 132.0f;

// Bookmarks: Note input.
const float BookmarksNoteCol = 44.0f;

// Disassembly list column starts (address | bytes | mnemonic | operands).
const float DisasmAddrW = 72.0f;
const float DisasmBytesW = 176.0f;
const float DisasmMnemW = 80.0f;

// Popups: default input width (e.g. Goto).
const float PopupInputWidth = 220.0f;

// Inspector table: type column width.
const float InspectorTypeCol = 110.0f;

// Vertical gap between sections (matches ImGui ItemSpacing.y * 1).
const float SectionGap = 8.0f;

// ImHex-aligned style values (match setupTheme); use for PushStyleVar when needed.
namespace Style {
  const float WindowPaddingX = 8.0f;
  const float WindowPaddingY = 8.0f;
  const float FramePaddingX = 4.0f;
  const float FramePaddingY = 3.0f;
  const float ItemSpacingX = 8.0f;
  const float ItemSpacingY = 4.0f;
  const float CellPaddingX = 4.0f;
  const float CellPaddingY = 2.0f;
}

// ─── Form table helpers ───────────────────────────────────────────────────
// Use for label/value blocks (Info, Hashes, etc.) so alignment is automatic.

inline bool BeginFormTable(const char* id) {
  return ImGui::BeginTable(id, 2, ImGuiTableFlags_None);
}

inline void FormTableSetupColumns() {
  ImGui::TableSetupColumn("L", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
  ImGui::TableSetupColumn("V");
}

inline void FormRow(const char* label, const char* value,
  float labelColorR = 0.53f, float labelColorG = 0.53f, float labelColorB = 0.53f,
  float valueColorR = 0.67f, float valueColorG = 0.67f, float valueColorB = 0.67f) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(ImVec4(labelColorR, labelColorG, labelColorB, 1.0f), "%s", label);
  ImGui::TableNextColumn();
  ImGui::TextColored(ImVec4(valueColorR, valueColorG, valueColorB, 1.0f), "%s", value);
}

inline void EndFormTable() {
  ImGui::EndTable();
}

} // namespace Design
