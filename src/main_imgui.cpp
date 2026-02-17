#include "core/app_state.hpp"
#include "core/backend.hpp"
#include "viz/digram.hpp"
#include "viz/trigram.hpp"
#include "ui/ui_panels.hpp"
#include "mac_pinch.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#endif
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#include <mach-o/dyld.h>
extern "C" {
const char* openFileDialog();
const char* saveFileDialog();
}
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>

FILE* g_logf = nullptr;

static void loadFile(const char* path) {
  LOG("[rever] loadFile('%s')...\n", path);
  g.bytes = load_file(path);
  LOG("[rever] loaded %zu bytes\n", g.bytes.size());
  if (g.bytes.empty()) {
    LOG("[rever] file empty or failed!\n");
    return;
  }
  g.filePath = path;
  const char* slash = strrchr(path, '/');
  g.fileName = slash ? slash + 1 : path;
  g.fileFormat = detect_format(g.bytes.data(), g.bytes.size());
  g.fileLoaded = true;
  g.hexCursor = 0;
  g.hexSelStart = 0;
  g.hexSelEnd = 0;
  g.hexModified.clear();

  g.disasmLines = disassemble(g.bytes.data(), g.bytes.size(), 0, g.disasmArch, 8000);

  LOG("[rever] computing histogram...\n");
  g.histogram = byte_histogram(g.bytes.data(), g.bytes.size());
  g.histMax = *std::max_element(g.histogram.begin(), g.histogram.end());
  LOG("[rever] computing entropy...\n");
  g.entropy = entropy_curve(g.bytes.data(), g.bytes.size());

  g.trigramVertCount = 0;
  g.trigramDirty = true;
  g.vizNeedsRebuild = true;
  g.vizRangeStart = 0;
  g.vizRangeEnd = 0;
  g.focusStart = 0;
  g.focusEnd = 0;
  LOG("[rever] file loaded, %zu bytes, GPU viz deferred\n", g.bytes.size());

  g.strings = extract_strings(g.bytes.data(), g.bytes.size(), g.stringsMinLen);
  g.sections = parse_sections(g.bytes.data(), g.bytes.size());
  g.imports = parse_imports(g.bytes.data(), g.bytes.size());
  g.exports = parse_exports(g.bytes.data(), g.bytes.size());
  g.md5Hash = hash_md5(g.bytes.data(), g.bytes.size());
  g.sha256Hash = hash_sha256(g.bytes.data(), g.bytes.size());
  g.fileInfo = get_file_info(g.bytes.data(), g.bytes.size());
  g.inspectorData = inspect_data(g.bytes.data(), g.bytes.size(), 0);
  g.searchHits.clear();
  g.searchCurrentHit = -1;
}

static void glfwDropCallback(GLFWwindow*, int count, const char* paths[]) {
  LOG("[rever] drop callback: count=%d\n", count);
  if (count > 0) {
    LOG("[rever] dropped file: %s\n", paths[0]);
    loadFile(paths[0]);
  }
}

int main(int argc, char** argv) {
  g_logf = fopen("/tmp/rever_debug.log", "w");
  if (g_logf) setvbuf(g_logf, nullptr, _IONBF, 0);
  LOG("[rever] starting...\n");

  if (!glfwInit()) {
    LOG("[rever] glfwInit failed!\n");
    return 1;
  }
  LOG("[rever] glfwInit OK\n");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  LOG("[rever] creating window...\n");
  GLFWwindow* window = glfwCreateWindow(1440, 900, "Rever", nullptr, nullptr);
  if (!window) {
    LOG("[rever] window creation failed!\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetDropCallback(window, glfwDropCallback);
#ifdef __APPLE__
  MacInstallPinchMonitor();
#endif

  LOG("[rever] ImGui init...\n");
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");

  float xscale = 1.0f, yscale = 1.0f;
  glfwGetWindowContentScale(window, &xscale, &yscale);
  float dpiScale = xscale;
  float logicalSize = 14.0f;
  float pixelSize = std::round(logicalSize * dpiScale);

  bool fontLoaded = false;
  {
#ifdef __APPLE__
    char exePath[1024] = {};
    uint32_t exeSize = sizeof(exePath);
    _NSGetExecutablePath(exePath, &exeSize);
    std::string exeDir(exePath);
    size_t sl = exeDir.rfind('/');
    if (sl != std::string::npos) exeDir = exeDir.substr(0, sl);
    std::string fontPath = exeDir + "/../Resources/RobotoMono.ttf";
#else
    std::string fontPath = std::string(REVER_PROJECT_DIR) + "/fonts/RobotoMono.ttf";
#endif
    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 1;
    cfg.RasterizerDensity = dpiScale;
    ImFont* f = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), logicalSize, &cfg);
    fontLoaded = (f != nullptr);
  }
  if (!fontLoaded) {
    ImFontConfig cfg;
    cfg.SizePixels = pixelSize;
    io.Fonts->AddFontDefault(&cfg);
    io.FontGlobalScale = 1.0f / dpiScale;
  }

  setupTheme();

  if (argc > 1) {
    loadFile(argv[1]);
  }

  bool firstFrame = true;
  bool secondFrame = false;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...", "Cmd+O")) g.showOpenPopup = true;
        if (ImGui::MenuItem("Save As...", "Cmd+Shift+S", false, g.fileLoaded)) {
          const char* path = saveFileDialog();
          if (path) save_file(path, g.bytes.data(), g.bytes.size());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Cmd+Q")) glfwSetWindowShouldClose(window, true);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Goto Address...", "Ctrl+G")) g.showGotoPopup = true;
        if (ImGui::MenuItem("Add Bookmark", "Ctrl+B", false, g.fileLoaded)) {
          size_t sa = std::min(g.hexSelStart, g.hexSelEnd);
          size_t sb = std::max(g.hexSelStart, g.hexSelEnd);
          g.bookmarks.push_back({sa != sb ? sa : g.hexCursor, sa != sb ? sb - sa + 1 : 1, "Bookmark"});
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::SeparatorText("Analysis");
        ImGui::MenuItem("Hex Editor", nullptr, &g.showHexEditor);
        ImGui::MenuItem("Disassembly", nullptr, &g.showDisassembly);
        ImGui::MenuItem("Strings", nullptr, &g.showStrings);
        ImGui::MenuItem("Search", nullptr, &g.showSearch);
        ImGui::SeparatorText("Visualization");
        ImGui::MenuItem("Trigram", nullptr, &g.showTrigram);
        if (ImGui::MenuItem("Trigram settings...")) g.openTrigramSettingsPopup = true;
        ImGui::MenuItem("Histogram", nullptr, &g.showHistogram);
        ImGui::MenuItem("Entropy", nullptr, &g.showEntropy);
        ImGui::MenuItem("Bigram", nullptr, &g.showBigram);
        ImGui::SeparatorText("Details");
        ImGui::MenuItem("Inspector", nullptr, &g.showInspector);
        ImGui::MenuItem("Sections", nullptr, &g.showSections);
        ImGui::MenuItem("Imports", nullptr, &g.showImports);
        ImGui::MenuItem("Exports", nullptr, &g.showExports);
        ImGui::MenuItem("Bookmarks", nullptr, &g.showBookmarks);
        ImGui::MenuItem("Info", nullptr, &g.showInfo);
        ImGui::Separator();
        if (ImGui::MenuItem("Show All")) {
          g.showHexEditor = g.showInspector = g.showDisassembly = true;
          g.showTrigram = g.showHistogram = g.showEntropy = g.showBigram = true;
          g.showStrings = g.showSections = g.showImports = g.showExports = true;
          g.showSearch = g.showBookmarks = g.showInfo = true;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    bool mod = io.KeySuper || io.KeyCtrl;
    if (mod && ImGui::IsKeyPressed(ImGuiKey_O)) g.showOpenPopup = true;
    if (mod && ImGui::IsKeyPressed(ImGuiKey_G)) g.showGotoPopup = true;
    if (mod && ImGui::IsKeyPressed(ImGuiKey_B) && g.fileLoaded) {
      size_t sa = std::min(g.hexSelStart, g.hexSelEnd);
      size_t sb = std::max(g.hexSelStart, g.hexSelEnd);
      g.bookmarks.push_back({sa != sb ? sa : g.hexCursor, sa != sb ? sb - sa + 1 : 1, "Bookmark"});
    }

    if (g.showOpenPopup) {
      g.showOpenPopup = false;
      const char* path = openFileDialog();
      if (path) loadFile(path);
    }

    if (g.vizNeedsRebuild && g.fileLoaded) {
      g.vizNeedsRebuild = false;
      if (g.trigramShader) {
        glDeleteProgram(g.trigramShader);
        g.trigramShader = 0;
      }
      initTrigramGL();
      buildTrigramGeometry();
      initDigramGL();
      buildDigramTexture();
    }

    drawGotoPopup();

    ImGuiID dockId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    if (firstFrame) {
      firstFrame = false;
      secondFrame = true;
    } else if (secondFrame) {
      secondFrame = false;
      ImGui::DockBuilderRemoveNode(dockId);
      ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->Size);
      ImGuiID dockMain = dockId;
      ImGuiID dockBottom, dockRight, dockLeftBottom;
      ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, &dockBottom, &dockMain);
      ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, &dockRight, &dockMain);
      ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.4f, &dockLeftBottom, &dockMain);
      ImGui::DockBuilderDockWindow("Hex Editor", dockMain);
      ImGui::DockBuilderDockWindow("Disassembly", dockLeftBottom);
      ImGui::DockBuilderDockWindow("Strings", dockLeftBottom);
      ImGui::DockBuilderDockWindow("Search", dockLeftBottom);
      ImGui::DockBuilderDockWindow("Inspector", dockRight);
      ImGui::DockBuilderDockWindow("Sections", dockRight);
      ImGui::DockBuilderDockWindow("Imports", dockRight);
      ImGui::DockBuilderDockWindow("Exports", dockRight);
      ImGui::DockBuilderDockWindow("Bookmarks", dockRight);
      ImGui::DockBuilderDockWindow("Info", dockRight);
      ImGui::DockBuilderDockWindow("Trigram", dockBottom);
      ImGui::DockBuilderDockWindow("Histogram", dockBottom);
      ImGui::DockBuilderDockWindow("Entropy", dockBottom);
      ImGui::DockBuilderDockWindow("Bigram", dockBottom);
      ImGui::DockBuilderFinish(dockId);
    }

    if (g.showHexEditor) drawHexEditor();
    if (g.showInspector) drawInspector();
    if (g.showDisassembly) drawDisassembly();
    if (g.showTrigram) drawTrigram();
    if (g.showHistogram) drawHistogram();
    if (g.showEntropy) drawEntropy();
    if (g.showBigram) drawBigram();
    if (g.showStrings) drawStrings();
    if (g.showSections) drawSections();
    if (g.showImports) drawImports();
    if (g.showExports) drawExports();
    if (g.showSearch) drawSearch();
    if (g.showBookmarks) drawBookmarks();
    if (g.showInfo) drawInfo();

    if (g.fileLoaded) {
      char title[256];
      snprintf(title, sizeof(title), "Rever — %s (%s)", g.fileName.c_str(), g.fileFormat.c_str());
      glfwSetWindowTitle(window, title);
    }

    ImGui::Render();
    int dw, dh;
    glfwGetFramebufferSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  if (g_logf) fclose(g_logf);
  return 0;
}
