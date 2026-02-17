#include "ui/theme_loader.hpp"
#include "imgui.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#else
#include <unistd.h>
#endif

#ifdef REVER_PROJECT_DIR
#define REVER_PROJECT_DIR_STR REVER_PROJECT_DIR
#else
#define REVER_PROJECT_DIR_STR ""
#endif

namespace {

enum Section { kNone, kStyle, kColors };

bool parseRgba(const char* s, float* r, float* g, float* b, float* a) {
  int ri = 0, gi = 0, bi = 0;
  float af = 1.0f;
  if (std::strstr(s, "rgba(")) {
    if (std::sscanf(s, "rgba(%d,%d,%d,%f)", &ri, &gi, &bi, &af) >= 3) {
      *r = ri / 255.0f;
      *g = gi / 255.0f;
      *b = bi / 255.0f;
      *a = af;
      return true;
    }
  }
  return false;
}

// ImGuiCol indices must match imgui.h (docking branch order).
int imguiColFromName(const char* name) {
  static const struct { const char* key; int col; } kMap[] = {
    {"Text", 0}, {"TextDisabled", 1}, {"WindowBg", 2}, {"ChildBg", 3}, {"PopupBg", 4},
    {"Border", 5}, {"BorderShadow", 6}, {"FrameBg", 7}, {"FrameBgHovered", 8}, {"FrameBgActive", 9},
    {"TitleBg", 10}, {"TitleBgActive", 11}, {"TitleBgCollapsed", 12}, {"MenuBarBg", 13},
    {"ScrollbarBg", 14}, {"ScrollbarGrab", 15}, {"ScrollbarGrabHovered", 16}, {"ScrollbarGrabActive", 17},
    {"CheckMark", 18}, {"SliderGrab", 19}, {"SliderGrabActive", 20},
    {"Button", 21}, {"ButtonHovered", 22}, {"ButtonActive", 23},
    {"Header", 24}, {"HeaderHovered", 25}, {"HeaderActive", 26},
    {"Separator", 27}, {"SeparatorHovered", 28}, {"SeparatorActive", 29},
    {"ResizeGrip", 30}, {"ResizeGripHovered", 31}, {"ResizeGripActive", 32},
    {"TabHovered", 33}, {"Tab", 34}, {"TabSelected", 35}, {"TabSelectedOverline", 36},
    {"TabDimmed", 37}, {"TabDimmedSelected", 38}, {"TabDimmedSelectedOverline", 39},
    {"DockingPreview", 40}, {"DockingEmptyBg", 41},
    {"PlotLines", 42}, {"PlotLinesHovered", 43}, {"PlotHistogram", 44}, {"PlotHistogramHovered", 45},
    {"TableHeaderBg", 46}, {"TableBorderStrong", 47}, {"TableBorderLight", 48}, {"TableRowBg", 49}, {"TableRowBgAlt", 50},
    {"TextLink", 51}, {"TextSelectedBg", 52}, {"DragDropTarget", 53},
    {"NavCursor", 54}, {"NavWindowingHighlight", 55}, {"NavWindowingDimBg", 56}, {"ModalWindowDimBg", 57},
    {"TabActive", 35}, {"TabUnfocused", 37}, {"TabUnfocusedActive", 38}, {"NavHighlight", 54},
  };
  for (const auto& e : kMap)
    if (std::strcmp(e.key, name) == 0)
      return e.col;
  return -1;
}

// Trim leading/trailing space; strip double or single quotes from value.
static void trimAndUnquote(std::string& s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    s = s.substr(1, s.size() - 2);
  else if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'')
    s = s.substr(1, s.size() - 2);
}

// Strip TOML comment from value (first # not inside double-quotes).
static void stripComment(std::string& s) {
  bool in_dq = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '"' && (i == 0 || s[i - 1] != '\\'))
      in_dq = !in_dq;
    else if (!in_dq && s[i] == '#') {
      s.resize(i);
      return;
    }
  }
}

// Parse "key = value" or "key = [x, y]"; return value part (trimmed).
static bool parseKeyValue(const std::string& line, std::string& key, std::string& value) {
  size_t eq = line.find('=');
  if (eq == std::string::npos) return false;
  key = line.substr(0, eq);
  value = line.substr(eq + 1);
  stripComment(value);
  trimAndUnquote(key);
  trimAndUnquote(value);
  return !key.empty();
}

static void applyStyleValue(ImGuiStyle& s, const std::string& key, const std::string& value) {
  float x = 0, y = 0;
  if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
    std::sscanf(value.c_str(), "[%f,%f]", &x, &y);
  } else {
    x = static_cast<float>(std::atof(value.c_str()));
    y = x;
  }
  if (key == "alpha") s.Alpha = x;
  else if (key == "disabledAlpha") s.DisabledAlpha = x;
  else if (key == "windowPadding") { s.WindowPadding.x = x; s.WindowPadding.y = y; }
  else if (key == "windowRounding") s.WindowRounding = x;
  else if (key == "windowBorderSize") s.WindowBorderSize = x;
  else if (key == "windowMinSize") { s.WindowMinSize.x = x; s.WindowMinSize.y = y; }
  else if (key == "windowTitleAlign") { s.WindowTitleAlign.x = x; s.WindowTitleAlign.y = y; }
  else if (key == "childRounding") s.ChildRounding = x;
  else if (key == "childBorderSize") s.ChildBorderSize = x;
  else if (key == "popupRounding") s.PopupRounding = x;
  else if (key == "popupBorderSize") s.PopupBorderSize = x;
  else if (key == "framePadding") { s.FramePadding.x = x; s.FramePadding.y = y; }
  else if (key == "frameRounding") s.FrameRounding = x;
  else if (key == "frameBorderSize") s.FrameBorderSize = x;
  else if (key == "itemSpacing") { s.ItemSpacing.x = x; s.ItemSpacing.y = y; }
  else if (key == "itemInnerSpacing") { s.ItemInnerSpacing.x = x; s.ItemInnerSpacing.y = y; }
  else if (key == "cellPadding") { s.CellPadding.x = x; s.CellPadding.y = y; }
  else if (key == "indentSpacing") s.IndentSpacing = x;
  else if (key == "scrollbarSize") s.ScrollbarSize = x;
  else if (key == "scrollbarRounding") s.ScrollbarRounding = x;
  else if (key == "grabMinSize") s.GrabMinSize = x;
  else if (key == "grabRounding") s.GrabRounding = x;
  else if (key == "tabRounding") s.TabRounding = x;
  else if (key == "tabBorderSize") s.TabBorderSize = x;
}

} // namespace

void applyImHexDesignLanguage() {
  ImGuiStyle& s = ImGui::GetStyle();
  s.Alpha = 1.0f;
  s.DisabledAlpha = 0.6f;
  s.WindowPadding = ImVec2(8.0f, 8.0f);
  s.WindowRounding = 0.0f;
  s.WindowBorderSize = 1.0f;
  s.WindowMinSize = ImVec2(32.0f, 32.0f);
  s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  s.ChildRounding = 0.0f;
  s.ChildBorderSize = 1.0f;
  s.PopupRounding = 0.0f;
  s.PopupBorderSize = 1.0f;
  s.FramePadding = ImVec2(4.0f, 3.0f);
  s.FrameRounding = 0.0f;
  s.FrameBorderSize = 0.0f;
  s.ItemSpacing = ImVec2(8.0f, 4.0f);
  s.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
  s.CellPadding = ImVec2(4.0f, 2.0f);
  s.IndentSpacing = 21.0f;
  s.ScrollbarSize = 14.0f;
  s.ScrollbarRounding = 9.0f;
  s.GrabMinSize = 12.0f;
  s.GrabRounding = 0.0f;
  s.TabRounding = 5.0f;
  s.TabBorderSize = 1.0f;  // Tabs have a border by default
  s.TabMinWidthForCloseButton = 0.0f;  // Show close button when hovering even on narrow tabs (ImGui 1.91.9+ has TabCloseButtonMinWidth* for always visible)
  s.SeparatorTextBorderSize = 3.0f;
  s.DockingSeparatorSize = 2.0f;
}

std::string getThemesTomlPath() {
  char buf[4096];
  buf[0] = '\0';
#ifdef _WIN32
  (void)GetModuleFileNameA(nullptr, buf, sizeof(buf));
#elif defined(__APPLE__)
  uint32_t len = sizeof(buf);
  if (_NSGetExecutablePath(buf, &len) != 0) buf[0] = '\0';
#else
  if (readlink("/proc/self/exe", buf, sizeof(buf)) < 0) buf[0] = '\0';
#endif
  if (buf[0]) {
    std::string exeDir = buf;
    size_t last = exeDir.find_last_of("/\\");
    if (last != std::string::npos) exeDir.resize(last + 1);
#ifdef __APPLE__
    // When running from .app, prefer bundle Resources (CMake copies themes.toml there)
    if (exeDir.find("/Contents/MacOS/") != std::string::npos) {
      std::string resources = exeDir + "../Resources/themes.toml";
      std::ifstream fr(resources);
      if (fr.good()) {
        char resolved[PATH_MAX];
        if (realpath(resources.c_str(), resolved))
          return resolved;
        return resources;
      }
    }
#endif
    std::string nextTo = exeDir + "themes.toml";
    std::ifstream f(nextTo);
    if (f.good()) return nextTo;
#ifdef __APPLE__
    std::string resources = exeDir + "../Resources/themes.toml";
    std::ifstream fr2(resources);
    if (fr2.good()) {
      char resolved[PATH_MAX];
      if (realpath(resources.c_str(), resolved))
        return resolved;
      return resources;
    }
#endif
  }
#ifdef REVER_PROJECT_DIR_STR
  std::string projectPath = REVER_PROJECT_DIR_STR;
  if (!projectPath.empty()) {
    std::string candidate = projectPath + "/themes.toml";
    std::ifstream f(candidate);
    if (f.good()) return candidate;
  }
#endif
  return "themes.toml";
}

static void themeLog(const char* fmt, ...) {
  (void)fmt;
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  (void)std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  (void)fprintf(stderr, "%s", buf);
  FILE* f = std::fopen("/tmp/rever_theme.log", "a");
  if (f) { (void)std::fprintf(f, "%s", buf); (void)std::fclose(f); }
}

bool applyThemeFromToml(const std::string& themeName) {
  std::string path = getThemesTomlPath();
  std::ifstream file(path);
  if (!file.good()) {
    themeLog("[rever] theme: file not found '%s', using built-in\n", path.c_str());
    return false;
  }

  std::string content;
  file.seekg(0, std::ios::end);
  size_t size = static_cast<size_t>(file.tellg());
  if (size == 0 || size > 1024 * 1024) {
    themeLog("[rever] theme: file empty or too large (%zu bytes), using built-in\n", size);
    return false;
  }
  content.resize(size);
  file.seekg(0);
  file.read(&content[0], static_cast<std::streamsize>(size));
  file.close();

  Section section = kNone;
  std::string currentName;
  bool inMatchingTheme = false;
  ImGuiStyle* s = nullptr;
  int styleCount = 0, colorCount = 0;

  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // Trim leading and trailing space; strip trailing comment for section detection
    size_t start = line.find_first_not_of(" \t");
    std::string trimmed = start == std::string::npos ? line : line.substr(start);
    size_t hash = trimmed.find('#');
    if (hash != std::string::npos) trimmed.resize(hash);
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();

    if (trimmed == "[[themes]]") {
      section = kNone;
      currentName.clear();
      inMatchingTheme = false;
      s = nullptr;
      continue;
    }
    if (trimmed == "[themes.style]") {
      section = kStyle;
      continue;
    }
    if (trimmed == "[themes.style.colors]") {
      section = kColors;
      continue;
    }

    std::string key, value;
    if (!parseKeyValue(line, key, value)) continue;

    if (section == kNone && key == "name") {
      currentName = value;
      if (currentName == themeName) {
        inMatchingTheme = true;
        s = &ImGui::GetStyle();
      }
      continue;
    }

    if (!inMatchingTheme || !s) continue;

    if (section == kStyle) {
      applyStyleValue(*s, key, value);
      ++styleCount;
    } else if (section == kColors) {
      int col = imguiColFromName(key.c_str());
      if (col >= 0 && col < ImGuiCol_COUNT) {
        float r, g, b, a;
        if (parseRgba(value.c_str(), &r, &g, &b, &a)) {
          s->Colors[col] = ImVec4(r, g, b, a);
          ++colorCount;
        }
      }
    }
  }

  // We only increment styleCount/colorCount when inMatchingTheme is true, so if we applied
  // anything we found and applied the theme (inMatchingTheme gets reset when we see next [[themes]]).
  if (styleCount > 0 || colorCount > 0) {
    themeLog("[rever] theme applied: '%s' from %s (style=%d color=%d)\n",
             themeName.c_str(), path.c_str(), styleCount, colorCount);
    return true;
  }
  themeLog("[rever] theme '%s' not applied (found=%d style=%d color=%d) from %s, using built-in\n",
           themeName.c_str(), inMatchingTheme ? 1 : 0, styleCount, colorCount, path.c_str());
  return false;
}

std::vector<std::string> getThemeNamesFromToml() {
  std::vector<std::string> names;
  std::string path = getThemesTomlPath();
  std::ifstream file(path);
  if (!file.good()) return names;

  std::string content;
  file.seekg(0, std::ios::end);
  size_t size = static_cast<size_t>(file.tellg());
  if (size == 0 || size > 1024 * 1024) return names;
  content.resize(size);
  file.seekg(0);
  file.read(&content[0], static_cast<std::streamsize>(size));
  file.close();

  Section section = kNone;
  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    size_t start = line.find_first_not_of(" \t");
    std::string trimmed = start == std::string::npos ? line : line.substr(start);

    if (trimmed == "[[themes]]") {
      section = kNone;
      continue;
    }
    if (trimmed == "[themes.style]") {
      section = kStyle;
      continue;
    }
    if (trimmed == "[themes.style.colors]") {
      section = kColors;
      continue;
    }

    std::string key, value;
    if (!parseKeyValue(line, key, value)) continue;
    if (section == kNone && key == "name") {
      names.push_back(value);
      continue;
    }
  }
  return names;
}
