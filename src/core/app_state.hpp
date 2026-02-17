#pragma once
#include "core/backend.hpp"
#include <cstdio>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct AppState {
  std::vector<uint8_t> bytes;
  std::string filePath;
  std::string fileName;
  std::string fileFormat;
  bool fileLoaded = false;

  // Hex view
  size_t hexCursor = 0;
  size_t hexSelStart = 0;
  size_t hexSelEnd = 0;
  bool hexEditing = false;
  bool hexHighNibble = true;
  std::set<size_t> hexModified;

  // Disassembly
  std::vector<DisasmLine> disasmLines;
  int disasmArch = 0;
  char disasmBaseAddr[32] = "0";

  // Visualization data
  std::array<uint32_t, 256> histogram{};
  uint32_t histMax = 0;
  std::vector<float> entropy;

  // Trigram (3D point cloud)
  GLuint trigramVAO = 0, trigramVBO = 0;
  GLuint trigramShader = 0;
  int trigramVertCount = 0;
  float trigramRotX = -20.0f, trigramRotY = 45.0f;
  float trigramZoom = 3.0f;
  float trigramBrightness = 50.0f;
  bool trigramDirty = true;
  bool trigramAutoRotate = true;
  bool trigramUserDragging = false;
  GLuint trigramFBO = 0, trigramRenderTex = 0, trigramDepthRBO = 0;
  GLuint trigramCompositeFBO = 0, trigramCompositeTex = 0;
  int trigramFBOW = 0, trigramFBOH = 0;
  int trigramShape = 0;
  float trigramCyl = 0.0f, trigramSph = 0.0f;
  int trigramColorMode = 1;  // 0 = single, 1 = Fyre (violet->..->orange, no green)
  bool trigramInvertColors = false;
  int trigramBrightnessMode = 0;  // 0 = Veles (file-scaled, gradient compresses at low brightness), 1 = Uniform (gradient preserved, brightness scales whole image)
  float trigramPointSize = 1.0f;
  uint64_t trigramMaxDensity = 0;

  // Bigram / Digram
  int bigramColorMode = 0;
  uint64_t digramMaxCount = 0;
  GLuint digramTex = 0;
  GLuint digramFBO = 0, digramRenderTex = 0;
  GLuint digramShader = 0;
  GLuint digramVAO = 0, digramVBO = 0;
  GLuint bigramTex = 0;
  std::vector<uint8_t> bigramRGBA;

  // Strings
  std::vector<FoundString> strings;
  int stringsMinLen = 4;
  char stringsFilter[256] = "";

  // Sections
  std::vector<Section> sections;

  // Imports / Exports
  std::vector<ImportEntry> imports;
  std::vector<ExportEntry> exports;
  char importsFilter[256] = "";

  // Search
  int searchMode = 0;
  char searchPattern[512] = "";
  bool searchCaseSensitive = true;
  std::vector<SearchHit> searchHits;
  int searchCurrentHit = -1;
  bool searchShowAll = false;

  // Hashes / Info
  std::string md5Hash;
  std::string sha256Hash;
  FileInfo fileInfo{};

  // Data inspector
  std::vector<DataInspectorResult> inspectorData;

  // Bookmarks
  struct Bookmark { size_t offset; size_t size; std::string note; };
  std::vector<Bookmark> bookmarks;
  char bookmarkNote[256] = "";

  bool vizNeedsRebuild = false;
  bool showGotoPopup = false;
  char gotoAddr[32] = "";
  bool showOpenPopup = false;
  char openPath[1024] = "";

  // Panel visibility
  bool showHexEditor = true;
  bool showInspector = true;
  bool showDisassembly = true;
  bool showTrigram = true;
  bool showHistogram = true;
  bool showEntropy = true;
  bool showBigram = true;
  bool showStrings = true;
  bool showSections = true;
  bool showImports = true;
  bool showExports = true;
  bool showSearch = true;
  bool showBookmarks = true;
  bool showInfo = true;
};

extern AppState g;
extern FILE* g_logf;
#define LOG(...) do { if (g_logf) { fprintf(g_logf, __VA_ARGS__); } } while (0)
