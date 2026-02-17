#include "ui/ui_panels.hpp"
#include "imgui.h"
#include "ui/design.hpp"
#include "ui/theme_loader.hpp"
#include "core/app_state.hpp"
#include "core/backend.hpp"
#include "ui/ui_legends.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Apply theme by name from themes.toml; fallback to built-in Rever style if not found.
// ImHex design language (layout/spacing/rounding) is always applied at the end; only colors vary by theme.
// Returns true if theme was applied from TOML, false if using built-in.
bool setupTheme() {
  if (applyThemeFromToml(g.themeName)) {
    applyImHexDesignLanguage();
    if (g.themeName == "Rever") {
      ImGuiStyle& s = ImGui::GetStyle();
      s.WindowRounding = s.ChildRounding = s.PopupRounding = 0.0f;
      s.FrameRounding = s.ScrollbarRounding = s.GrabRounding = s.TabRounding = 0.0f;
      // Dock and inactive tab borders dark; only active tab white (bg + overline)
      ImVec4* c = ImGui::GetStyle().Colors;
      c[ImGuiCol_Border] = ImVec4(0.27f, 0.27f, 0.27f, 1.0f);
      c[ImGuiCol_TabSelected] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      c[ImGuiCol_TabSelectedOverline] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return true;
  }
  // Fallback: built-in dark colors (theme controls colors; design language applied below)
  ImGuiStyle& s = ImGui::GetStyle();
  ImVec4* c = s.Colors;
  ImVec4 black(0, 0, 0, 1);
  ImVec4 border(0.27f, 0.27f, 0.27f, 1);
  ImVec4 textMain(0.8f, 0.8f, 0.8f, 1);
  ImVec4 textDim(0.4f, 0.4f, 0.4f, 1);
  ImVec4 selected(0.8f, 0.8f, 0.8f, 1);
  ImVec4 hover(0.13f, 0.13f, 0.13f, 1);
  ImVec4 active(0.2f, 0.2f, 0.2f, 1);
  ImVec4 scrollbar(0.33f, 0.33f, 0.33f, 1);

  c[ImGuiCol_Text] = textMain;
  c[ImGuiCol_TextDisabled] = textDim;
  c[ImGuiCol_WindowBg] = black;
  c[ImGuiCol_ChildBg] = black;
  c[ImGuiCol_PopupBg] = ImVec4(0.03f, 0.03f, 0.03f, 1);
  c[ImGuiCol_Border] = border;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = ImVec4(0.04f, 0.04f, 0.04f, 1);
  c[ImGuiCol_FrameBgHovered] = hover;
  c[ImGuiCol_FrameBgActive] = active;
  c[ImGuiCol_TitleBg] = black;
  c[ImGuiCol_TitleBgActive] = black;
  c[ImGuiCol_TitleBgCollapsed] = black;
  c[ImGuiCol_MenuBarBg] = black;
  c[ImGuiCol_ScrollbarBg] = black;
  c[ImGuiCol_ScrollbarGrab] = scrollbar;
  c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1);
  c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1);
  c[ImGuiCol_CheckMark] = selected;
  c[ImGuiCol_SliderGrab] = scrollbar;
  c[ImGuiCol_SliderGrabActive] = selected;
  c[ImGuiCol_Button] = ImVec4(0.07f, 0.07f, 0.07f, 1);
  c[ImGuiCol_ButtonHovered] = hover;
  c[ImGuiCol_ButtonActive] = active;
  c[ImGuiCol_Header] = ImVec4(0.1f, 0.1f, 0.1f, 1);
  c[ImGuiCol_HeaderHovered] = hover;
  c[ImGuiCol_HeaderActive] = active;
  c[ImGuiCol_Separator] = border;
  c[ImGuiCol_SeparatorHovered] = ImVec4(0.5f, 0.5f, 0.5f, 1);
  c[ImGuiCol_SeparatorActive] = selected;
  c[ImGuiCol_ResizeGrip] = ImVec4(0.15f, 0.15f, 0.15f, 1);
  c[ImGuiCol_ResizeGripHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1);
  c[ImGuiCol_ResizeGripActive] = ImVec4(0.5f, 0.5f, 0.5f, 1);
  c[ImGuiCol_Tab] = black;
  c[ImGuiCol_TabHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1);
  c[ImGuiCol_TabSelected] = ImVec4(1.0f, 1.0f, 1.0f, 1);       // Active tab in active dock: white background
  c[ImGuiCol_TabSelectedOverline] = ImVec4(1.0f, 1.0f, 1.0f, 1);  // White overline on selected tab
  c[ImGuiCol_TabDimmed] = black;
  c[ImGuiCol_TabDimmedSelected] = ImVec4(0.15f, 0.15f, 0.15f, 1);
  // Border stays dark (set above); active tab uses TabSelected (white bg) + TabSelectedOverline (white)
  c[ImGuiCol_DockingPreview] = ImVec4(0.4f, 0.4f, 0.4f, 0.7f);
  c[ImGuiCol_DockingEmptyBg] = black;
  c[ImGuiCol_TableHeaderBg] = ImVec4(0.05f, 0.05f, 0.05f, 1);
  c[ImGuiCol_TableBorderStrong] = border;
  c[ImGuiCol_TableBorderLight] = ImVec4(0.15f, 0.15f, 0.15f, 1);
  c[ImGuiCol_TableRowBg] = black;
  c[ImGuiCol_TableRowBgAlt] = ImVec4(0.03f, 0.03f, 0.03f, 1);
  c[ImGuiCol_TextSelectedBg] = active;
  c[ImGuiCol_NavHighlight] = selected;
  applyImHexDesignLanguage();
  // Tab styling already set above (white border, white active tab)
  if (g.themeName == "Rever") {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = s.ChildRounding = s.PopupRounding = 0.0f;
    s.FrameRounding = s.ScrollbarRounding = s.GrabRounding = s.TabRounding = 0.0f;
  }
  return false;
}

void drawHexEditor() {
  if (!ImGui::Begin("Hex Editor", &g.showHexEditor)) { ImGui::End(); return; }
  if (!g.fileLoaded) {
    ImGui::TextColored(ImVec4(0.2f,0.2f,0.2f,1), "Open a file (Cmd+O)");
    ImGui::End(); return;
  }
  const int bpl = 16;
  float cw = ImGui::CalcTextSize("0").x;
  float lh = ImGui::GetTextLineHeightWithSpacing();
  size_t totalLines = (g.bytes.size() + bpl - 1) / bpl;
  const float addrW = cw * 8.5f;
  const float hexByteW = cw * 2.2f;
  const float midGap = cw * 1.2f;
  const float hexStart = addrW + midGap;
  const float asciiStart = hexStart + 8 * hexByteW + midGap + 8 * hexByteW + midGap;
  ImGui::TextColored(ImVec4(0.27f,0.27f,0.27f,1), "Offset");
  for (int i = 0; i < 16; ++i) {
    float x = hexStart + (i < 8 ? i : 8 + (i - 8)) * hexByteW;
    if (i == 8) x = hexStart + 8 * hexByteW + midGap;
    ImGui::SameLine(x);
    char buf[4]; snprintf(buf, sizeof(buf), "%02X", i);
    ImGui::TextColored(ImVec4(0.27f,0.27f,0.27f,1), "%s", buf);
  }
  ImGui::SameLine(asciiStart);
  ImGui::TextColored(ImVec4(0.27f,0.27f,0.27f,1), "ASCII");
  ImGui::Separator();
  ImGui::BeginChild("HexScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove);
  ImGuiListClipper clipper;
  clipper.Begin((int)totalLines, lh);
  size_t sa = std::min(g.hexSelStart, g.hexSelEnd);
  size_t sb = std::max(g.hexSelStart, g.hexSelEnd);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  while (clipper.Step()) {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
      size_t off = (size_t)line * bpl;
      ImVec2 linePos = ImGui::GetCursorScreenPos();
      if (g.hexCursor / 16 == (size_t)line)
        dl->AddRectFilled(linePos, ImVec2(linePos.x + ImGui::GetContentRegionAvail().x, linePos.y + lh),
                          IM_COL32(10, 10, 10, 255));
      char addr[16]; snprintf(addr, sizeof(addr), "%08zx", off);
      ImGui::TextColored(ImVec4(0.27f,0.27f,0.27f,1), "%s", addr);
      ImGui::SameLine(hexStart);
      for (int b = 0; b < bpl && off + b < g.bytes.size(); ++b) {
        uint8_t v = g.bytes[off + b];
        size_t addr2 = off + b;
        bool isCursor = (addr2 == g.hexCursor);
        bool inSel = (sa != sb && addr2 >= sa && addr2 <= sb);
        bool isMod = g.hexModified.count(addr2) > 0;
        if (b == 8) ImGui::SameLine(hexStart + 8 * hexByteW + midGap);
        if (isCursor || inSel) {
          ImVec2 p = ImGui::GetCursorScreenPos();
          ImU32 col = isCursor ? IM_COL32(51,51,51,255) : IM_COL32(26,26,46,255);
          dl->AddRectFilled(p, ImVec2(p.x + cw * 2.2f, p.y + lh), col);
        }
        char hex[4]; snprintf(hex, sizeof(hex), "%02x", v);
        ImVec4 color;
        if (isMod) color = ImVec4(0.8f, 0.4f, 0.4f, 1);
        else if (isCursor) color = ImVec4(1, 1, 1, 1);
        else if (inSel) color = ImVec4(0.8f, 0.8f, 0.8f, 1);
        else if (v == 0) color = ImVec4(0.17f, 0.17f, 0.17f, 1);
        else if (v >= 0x20 && v <= 0x7E) color = ImVec4(0.67f, 0.67f, 0.67f, 1);
        else color = ImVec4(0.33f, 0.33f, 0.33f, 1);
        ImGui::TextColored(color, "%s", hex);
        if (ImGui::IsItemClicked(0)) {
          if (ImGui::GetIO().KeyShift) { g.hexSelEnd = addr2; }
          else { g.hexSelStart = g.hexSelEnd = addr2; }
          g.hexCursor = addr2;
          g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
        }
        ImGui::SameLine();
      }
      int remaining = bpl - (int)(std::min(g.bytes.size() - off, (size_t)bpl));
      for (int b = 0; b < remaining; ++b) { ImGui::TextColored(ImVec4(0,0,0,1), "   "); ImGui::SameLine(); }
      ImGui::SameLine(asciiStart);
      char ascii[32]; memset(ascii, 0, sizeof(ascii));
      for (int b = 0; b < bpl && off + b < g.bytes.size(); ++b) {
        uint8_t v = g.bytes[off + b];
        ascii[b] = (v >= 0x20 && v <= 0x7E) ? (char)v : '.';
      }
      ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.47f, 1), "%s", ascii);
    }
  }
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
    ImGuiIO& io = ImGui::GetIO();
    size_t sz = g.bytes.size();
    bool moved = false;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && g.hexCursor + 1 < sz) { g.hexCursor++; moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && g.hexCursor > 0) { g.hexCursor--; moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && g.hexCursor + 16 < sz) { g.hexCursor += 16; moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && g.hexCursor >= 16) { g.hexCursor -= 16; moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) { g.hexCursor = std::min(sz - 1, g.hexCursor + 256); moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) { g.hexCursor = g.hexCursor > 256 ? g.hexCursor - 256 : 0; moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) { g.hexCursor = 0; moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_End)) { g.hexCursor = sz - 1; moved = true; }
    if (moved) {
      if (io.KeyShift) g.hexSelEnd = g.hexCursor;
      else g.hexSelStart = g.hexSelEnd = g.hexCursor;
      g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
    }
    for (int k = 0; k < io.InputQueueCharacters.Size; ++k) {
      ImWchar ch = io.InputQueueCharacters[k];
      int nib = -1;
      if (ch >= '0' && ch <= '9') nib = ch - '0';
      else if (ch >= 'a' && ch <= 'f') nib = ch - 'a' + 10;
      else if (ch >= 'A' && ch <= 'F') nib = ch - 'A' + 10;
      if (nib >= 0 && g.hexCursor < sz) {
        uint8_t old = g.bytes[g.hexCursor];
        if (g.hexHighNibble) {
          g.bytes[g.hexCursor] = (old & 0x0F) | ((uint8_t)nib << 4);
          g.hexHighNibble = false;
        } else {
          g.bytes[g.hexCursor] = (old & 0xF0) | (uint8_t)nib;
          g.hexHighNibble = true;
          g.hexModified.insert(g.hexCursor);
          if (g.hexCursor + 1 < sz) g.hexCursor++;
        }
        g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
      }
    }
  }
  ImGui::EndChild();
  ImGui::End();
}

void drawInspector() {
  if (!ImGui::Begin("Inspector", &g.showInspector)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  ImGui::TextColored(ImVec4(0.4f,0.4f,0.4f,1), "Offset: 0x%zx", g.hexCursor);
  ImGui::Separator();
  if (ImGui::BeginTable("##insp", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Design::InspectorTypeCol);
    ImGui::TableSetupColumn("Value");
    for (auto& r : g.inspectorData) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(ImVec4(0.4f,0.4f,0.4f,1), "%s", r.type_name.c_str());
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(ImVec4(0.67f,0.67f,0.67f,1), "%s", r.value.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void drawDisassembly() {
  if (!ImGui::Begin("Disassembly", &g.showDisassembly)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  const char* archNames[] = {"x86-64","x86-32","ARM64","ARM32","MIPS"};
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Arch");
  ImGui::SameLine(Design::ToolbarCol1);
  ImGui::SetNextItemWidth(100);
  if (ImGui::BeginCombo("##arch", archNames[g.disasmArch])) {
    for (int i = 0; i < 5; ++i)
      if (ImGui::Selectable(archNames[i], i == g.disasmArch)) g.disasmArch = i;
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Base");
  ImGui::SameLine(Design::ToolbarCol2);
  ImGui::SetNextItemWidth(90);
  ImGui::InputText("##base", g.disasmBaseAddr, sizeof(g.disasmBaseAddr));
  ImGui::SameLine();
  if (ImGui::Button("Disassemble")) {
    uint64_t base = strtoull(g.disasmBaseAddr, nullptr, 16);
    g.disasmLines = disassemble(g.bytes.data(), g.bytes.size(), base, g.disasmArch, 8000);
  }
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.33f,0.33f,0.33f,1), "%zu instructions", g.disasmLines.size());
  ImGui::Separator();
  ImGui::BeginChild("DisasmScroll");
  ImGuiListClipper clipper;
  clipper.Begin((int)g.disasmLines.size());
  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      auto& l = g.disasmLines[i];
      ImGui::TextColored(ImVec4(0.33f,0.33f,0.33f,1), "%08llx", (unsigned long long)l.address);
      ImGui::SameLine(Design::DisasmAddrW);
      ImGui::TextColored(ImVec4(0.27f,0.27f,0.27f,1), "%-24s", l.bytes_hex.c_str());
      ImGui::SameLine(Design::DisasmAddrW + Design::DisasmBytesW);
      bool isBranch = (l.mnemonic[0]=='j'||l.mnemonic=="call"||l.mnemonic=="ret"||
                        l.mnemonic=="syscall"||l.mnemonic[0]=='b');
      if (isBranch)
        ImGui::TextColored(ImVec4(0.67f,0.47f,0.47f,1), "%-8s", l.mnemonic.c_str());
      else
        ImGui::TextColored(ImVec4(0.53f,0.53f,0.73f,1), "%-8s", l.mnemonic.c_str());
      ImGui::SameLine(Design::DisasmAddrW + Design::DisasmBytesW + Design::DisasmMnemW);
      ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "%s", l.operands.c_str());
    }
  }
  ImGui::EndChild();
  ImGui::End();
}

void drawHistogram() {
  if (!ImGui::Begin("Histogram", &g.showHistogram)) { ImGui::End(); return; }
  if (!g.fileLoaded || g.histMax == 0) { ImGui::TextDisabled("No data"); ImGui::End(); return; }
  static bool logScale = false;
  ImGui::Checkbox("Log scale", &logScale);
  ImGui::SameLine(0, 20);
  {
    ImVec2 barPos = ImGui::GetCursorScreenPos();
    barPos.y += 2;
    float barW = std::min(ImGui::GetContentRegionAvail().x - 10.0f, 200.0f);
    float barH = 10.0f;
    ImDrawList* dlLeg = ImGui::GetWindowDrawList();
    drawLegendBar(dlLeg, barPos, barW, barH, histogramLegendColor, 0.0f, "0x00", "0xFF");
    ImGui::Dummy(ImVec2(barW, barH + 2));
  }
  ImVec2 avail = ImGui::GetContentRegionAvail();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  float w = avail.x, h = avail.y - 16;
  if (h < 10) { ImGui::Dummy(avail); ImGui::End(); return; }
  float bw = w / 256.f;
  float logMax = logScale ? log2f((float)g.histMax + 1.0f) : 0;
  for (int i = 1; i < 4; ++i) {
    float y = origin.y + h - h * i / 4.f;
    dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + w, y), IM_COL32(20, 20, 20, 255));
  }
  for (int i = 0; i < 256; ++i) {
    float v = logScale ? log2f((float)g.histogram[i] + 1.0f) / logMax : (float)g.histogram[i] / (float)g.histMax;
    float bh = v * h;
    float x = origin.x + i * bw;
    float t = (float)i / 255.f;
    uint8_t r = (uint8_t)(40 + 30 * t);
    uint8_t gn = (uint8_t)(60 + 100 * t);
    uint8_t b = (uint8_t)(130 * (1.0f - t) + 40 * t);
    dl->AddRectFilled(ImVec2(x, origin.y + h - bh), ImVec2(x + std::max(1.f, bw - 0.5f), origin.y + h),
                      IM_COL32(r, gn, b, 200));
  }
  dl->AddText(ImVec2(origin.x + 2, origin.y + h + 2), IM_COL32(68,68,68,255), "0x00");
  char midLbl[16]; snprintf(midLbl, sizeof(midLbl), "0x80");
  dl->AddText(ImVec2(origin.x + w * 0.5f - 10, origin.y + h + 2), IM_COL32(68,68,68,255), midLbl);
  dl->AddText(ImVec2(origin.x + w - 30, origin.y + h + 2), IM_COL32(68,68,68,255), "0xFF");
  ImGui::Dummy(avail);
  ImGui::End();
}

void drawEntropy() {
  if (!ImGui::Begin("Entropy", &g.showEntropy)) { ImGui::End(); return; }
  if (!g.fileLoaded || g.entropy.size() < 2) { ImGui::TextDisabled("No data"); ImGui::End(); return; }
  {
    ImVec2 barPos = ImGui::GetCursorScreenPos();
    float barW = std::min(ImGui::GetContentRegionAvail().x, 300.0f);
    float barH = 10.0f;
    ImDrawList* dlLeg = ImGui::GetWindowDrawList();
    drawLegendBar(dlLeg, barPos, barW, barH, entropyLegendColor, 0.0f, "Low entropy", "High entropy");
    ImGui::Dummy(ImVec2(barW, barH + 16));
  }
  ImVec2 avail = ImGui::GetContentRegionAvail();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  float w = avail.x, h = avail.y - 16;
  if (h < 10) { ImGui::Dummy(avail); ImGui::End(); return; }
  for (int i = 1; i <= 4; ++i) {
    float y = origin.y + h - h * i / 4.f;
    dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + w, y), IM_COL32(20, 20, 20, 255));
    char lbl[8]; snprintf(lbl, sizeof(lbl), "%d", i * 2);
    dl->AddText(ImVec2(origin.x + 2, y - 12), IM_COL32(40,40,40,255), lbl);
  }
  float xs = w / (float)(g.entropy.size() - 1);
  int n = (int)g.entropy.size();
  for (int i = 0; i + 1 < n; i++) {
    float x0 = origin.x + i * xs;
    float x1 = origin.x + (i + 1) * xs;
    float e0 = g.entropy[i] / 8.0f;
    float e1 = g.entropy[i + 1] / 8.0f;
    float y0 = origin.y + h - e0 * h;
    float y1 = origin.y + h - e1 * h;
    float yBot = origin.y + h;
    float eMid = (e0 + e1) * 0.5f;
    uint8_t r, gn, b;
    if (eMid < 0.5f) {
      r = (uint8_t)(eMid * 2.0f * 200);
      gn = (uint8_t)(80 + eMid * 2.0f * 80);
      b = (uint8_t)(60 * (1.0f - eMid * 2.0f));
    } else {
      float t = (eMid - 0.5f) * 2.0f;
      r = (uint8_t)(200 + t * 55);
      gn = (uint8_t)(160 * (1.0f - t) + 60 * t);
      b = (uint8_t)(20 * t);
    }
    ImVec2 quad[4] = { ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(x1, yBot), ImVec2(x0, yBot) };
    dl->AddQuadFilled(quad[0], quad[1], quad[2], quad[3], IM_COL32(r, gn, b, 60));
  }
  std::vector<ImVec2> pts;
  for (int i = 0; i < n; ++i) {
    float x = origin.x + i * xs;
    float y = origin.y + h - (g.entropy[i] / 8.f) * h;
    pts.push_back(ImVec2(x, y));
  }
  dl->AddPolyline(pts.data(), n, IM_COL32(200, 200, 200, 220), 0, 1.5f);
  dl->AddText(ImVec2(origin.x + 2, origin.y + 2), IM_COL32(50,50,50,255), "8 bits");
  ImGui::Dummy(avail);
  ImGui::End();
}

void drawStrings() {
  if (!ImGui::Begin("Strings", &g.showStrings)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Min length");
  ImGui::SameLine(Design::InlineLabel1Col);
  ImGui::SetNextItemWidth(48);
  if (ImGui::InputInt("##minlen", &g.stringsMinLen, 1, 10)) {
    if (g.stringsMinLen < 2) g.stringsMinLen = 2;
    g.strings = extract_strings(g.bytes.data(), g.bytes.size(), g.stringsMinLen);
  }
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Filter");
  ImGui::SameLine(Design::InlineLabel2Col);
  ImGui::SetNextItemWidth(-70);
  ImGui::InputText("##filter", g.stringsFilter, sizeof(g.stringsFilter));
  ImGui::SameLine();
  int count = 0;
  for (auto& s : g.strings) {
    if (g.stringsFilter[0] && !strstr(s.value.c_str(), g.stringsFilter)) continue;
    count++;
  }
  ImGui::TextColored(ImVec4(0.33f,0.33f,0.33f,1), "%d", count);
  ImGui::Separator();
  if (ImGui::BeginTable("##strs", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Len", ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("String");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (auto& s : g.strings) {
      if (g.stringsFilter[0] && !strstr(s.value.c_str(), g.stringsFilter)) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      char off[16]; snprintf(off, sizeof(off), "0x%zx", s.offset);
      if (ImGui::Selectable(off, false, ImGuiSelectableFlags_SpanAllColumns)) {
        g.hexCursor = s.offset; g.hexSelStart = g.hexSelEnd = s.offset;
        g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
      }
      ImGui::TableNextColumn(); ImGui::Text("%zu", s.value.size());
      ImGui::TableNextColumn(); ImGui::TextUnformatted(s.value.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void drawSections() {
  if (!ImGui::Begin("Sections", &g.showSections)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  if (ImGui::BeginTable("##secs", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("VAddr", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (auto& s : g.sections) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Selectable(s.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
        g.hexCursor = s.offset; g.hexSelStart = g.hexSelEnd = s.offset;
        g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
      }
      ImGui::TableNextColumn(); ImGui::Text("0x%llx", (unsigned long long)s.vaddr);
      ImGui::TableNextColumn(); ImGui::Text("0x%llx", (unsigned long long)s.offset);
      ImGui::TableNextColumn(); ImGui::Text("0x%llx", (unsigned long long)s.size);
      ImGui::TableNextColumn(); ImGui::Text("%s", s.flags.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void drawImports() {
  if (!ImGui::Begin("Imports", &g.showImports)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##impfilt", "Filter imports...", g.importsFilter, sizeof(g.importsFilter));
  ImGui::Separator();
  if (ImGui::BeginTable("##imps", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Library");
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Hint", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (auto& im : g.imports) {
      if (g.importsFilter[0] && !strstr(im.name.c_str(), g.importsFilter) && !strstr(im.library.c_str(), g.importsFilter)) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::TextUnformatted(im.library.c_str());
      ImGui::TableNextColumn(); ImGui::TextUnformatted(im.name.c_str());
      ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)im.hint);
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void drawExports() {
  if (!ImGui::Begin("Exports", &g.showExports)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  if (ImGui::BeginTable("##exps", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("RVA", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Ordinal", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (auto& e : g.exports) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::TextUnformatted(e.name.c_str());
      ImGui::TableNextColumn(); ImGui::Text("0x%llx", (unsigned long long)e.rva);
      ImGui::TableNextColumn(); ImGui::Text("%d", e.ordinal);
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void drawSearch() {
  if (!ImGui::Begin("Search", &g.showSearch)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  const char* modes[] = {"Text", "Hex"};
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Mode");
  ImGui::SameLine(Design::SearchModeCol);
  ImGui::SetNextItemWidth(64);
  ImGui::Combo("##mode", &g.searchMode, modes, 2);
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Pattern");
  ImGui::SameLine(Design::SearchPatternCol);
  ImGui::SetNextItemWidth(-200);
  bool enter = ImGui::InputText("##pat", g.searchPattern, sizeof(g.searchPattern), ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if (g.searchMode == 0) { ImGui::Checkbox("Case", &g.searchCaseSensitive); ImGui::SameLine(); }
  bool doSearch = enter || ImGui::Button("Next"); ImGui::SameLine();
  if (ImGui::Button("Prev")) {
    if (!g.searchHits.empty()) {
      g.searchCurrentHit = (g.searchCurrentHit - 1 + (int)g.searchHits.size()) % (int)g.searchHits.size();
      g.hexCursor = g.searchHits[g.searchCurrentHit].offset;
      g.hexSelStart = g.hexSelEnd = g.hexCursor;
      g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("All")) { g.searchShowAll = true; doSearch = true; }
  if (doSearch && g.searchPattern[0]) {
    if (g.searchMode == 0)
      g.searchHits = search_text(g.bytes.data(), g.bytes.size(), g.searchPattern, g.searchCaseSensitive);
    else {
      std::vector<uint8_t> pat;
      const char* p = g.searchPattern;
      while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char byte[3] = {p[0], p[1] ? p[1] : '\0', '\0'};
        pat.push_back((uint8_t)strtoul(byte, nullptr, 16));
        p += byte[1] ? 2 : 1;
      }
      g.searchHits = search_hex(g.bytes.data(), g.bytes.size(), pat.data(), pat.size());
    }
    if (!g.searchHits.empty()) {
      g.searchCurrentHit = (g.searchCurrentHit + 1) % (int)g.searchHits.size();
      g.hexCursor = g.searchHits[g.searchCurrentHit].offset;
      g.hexSelStart = g.hexSelEnd = g.hexCursor;
      g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
    }
  }
  ImGui::SameLine();
  if (g.searchHits.empty()) ImGui::TextColored(ImVec4(0.33f,0.33f,0.33f,1), "No matches");
  else ImGui::TextColored(ImVec4(0.33f,0.33f,0.33f,1), "%d/%zu", g.searchCurrentHit + 1, g.searchHits.size());
  if (g.searchShowAll && !g.searchHits.empty()) {
    ImGui::Separator();
    ImGui::BeginChild("SearchResults", ImVec2(0, 150));
    for (int i = 0; i < (int)g.searchHits.size(); ++i) {
      char lbl[32]; snprintf(lbl, sizeof(lbl), "0x%08zx", g.searchHits[i].offset);
      if (ImGui::Selectable(lbl, i == g.searchCurrentHit)) {
        g.searchCurrentHit = i;
        g.hexCursor = g.searchHits[i].offset;
        g.hexSelStart = g.hexSelEnd = g.hexCursor;
        g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();
}

void drawBookmarks() {
  if (!ImGui::Begin("Bookmarks", &g.showBookmarks)) { ImGui::End(); return; }
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Note");
  ImGui::SameLine(Design::BookmarksNoteCol);
  ImGui::SetNextItemWidth(-60);
  ImGui::InputTextWithHint("##note", "Note...", g.bookmarkNote, sizeof(g.bookmarkNote));
  ImGui::SameLine();
  if (ImGui::Button("+ Add")) {
    size_t sa = std::min(g.hexSelStart, g.hexSelEnd);
    size_t sb = std::max(g.hexSelStart, g.hexSelEnd);
    size_t sz = (sa != sb) ? sb - sa + 1 : 1;
    g.bookmarks.push_back({sa != sb ? sa : g.hexCursor, sz, g.bookmarkNote[0] ? g.bookmarkNote : "Bookmark"});
    g.bookmarkNote[0] = '\0';
  }
  ImGui::Separator();
  int toRemove = -1;
  if (ImGui::BeginTable("##bm", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Note");
    ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 20);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (int i = 0; i < (int)g.bookmarks.size(); ++i) {
      auto& b = g.bookmarks[i];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      char off[16]; snprintf(off, sizeof(off), "0x%zx", b.offset);
      if (ImGui::Selectable(off, false, ImGuiSelectableFlags_SpanAllColumns)) {
        g.hexCursor = b.offset; g.hexSelStart = g.hexSelEnd = b.offset;
        g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
      }
      ImGui::TableNextColumn(); ImGui::Text("0x%zx", b.size);
      ImGui::TableNextColumn(); ImGui::TextUnformatted(b.note.c_str());
      ImGui::TableNextColumn();
      ImGui::PushID(i);
      if (ImGui::SmallButton("x")) toRemove = i;
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  if (toRemove >= 0) g.bookmarks.erase(g.bookmarks.begin() + toRemove);
  ImGui::End();
}

void drawInfo() {
  if (!ImGui::Begin("Info", &g.showInfo)) { ImGui::End(); return; }
  if (!g.fileLoaded) { ImGui::TextDisabled("No file"); ImGui::End(); return; }
  ImGui::SeparatorText("File Info");
  if (Design::BeginFormTable("##fileinfo")) {
    Design::FormTableSetupColumns();
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", g.fileInfo.format.c_str());
    Design::FormRow("Format:", buf);
    snprintf(buf, sizeof(buf), "%s", g.fileInfo.arch.c_str());
    Design::FormRow("Arch:", buf);
    snprintf(buf, sizeof(buf), "%s", g.fileInfo.bits.c_str());
    Design::FormRow("Bits:", buf);
    snprintf(buf, sizeof(buf), "%s", g.fileInfo.endian.c_str());
    Design::FormRow("Endian:", buf);
    snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)g.fileInfo.entry_point);
    Design::FormRow("Entry:", buf);
    snprintf(buf, sizeof(buf), "%zu bytes", g.fileInfo.file_size);
    Design::FormRow("Size:", buf);
    Design::EndFormTable();
  }
  ImGui::SeparatorText("Hashes");
  if (Design::BeginFormTable("##hashes")) {
    Design::FormTableSetupColumns();
    Design::FormRow("MD5:", g.md5Hash.c_str());
    Design::FormRow("SHA-256:", g.sha256Hash.c_str());
    Design::EndFormTable();
  }
  ImGui::End();
}

void drawGotoPopup() {
  if (g.showGotoPopup) {
    ImGui::OpenPopup("Goto Address");
    g.showGotoPopup = false;
  }
  if (ImGui::BeginPopupModal("Goto Address", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter hex offset:");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(Design::PopupInputWidth);
    bool enter = ImGui::InputText("##goto", g.gotoAddr, sizeof(g.gotoAddr), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Spacing();
    if (enter || ImGui::Button("Go")) {
      size_t off = strtoull(g.gotoAddr, nullptr, 16);
      if (off < g.bytes.size()) {
        g.hexCursor = off; g.hexSelStart = g.hexSelEnd = off;
        g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), g.hexCursor);
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}
