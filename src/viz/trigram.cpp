#include "viz/trigram.hpp"
#include "core/app_state.hpp"
#include "ui/design.hpp"
#include "viz/digram.hpp"
#include "viz/gl_helpers.hpp"
#include "viz/range_selector.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

// Draw a horizontal gradient strip for combo preview. mode: 0 = Single, 1 = Fyre.
static void drawGradientStrip(ImVec2 p_min, ImVec2 p_max, int mode) {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  auto col = [](float r, float g, float b) { return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255); };
  if (mode == 0) {
    dl->AddRectFilled(p_min, p_max, col(0.6f, 0.6f, 0.6f));
    return;
  }
  if (mode == 1) {
    // Fyre: violet -> blue -> lblue -> white -> yellow -> red; purple and red get more space (match shader)
    ImU32 stops[] = { col(0.58f, 0.f, 0.83f), col(0.2f, 0.5f, 1.f), col(0.4f, 0.75f, 1.f), col(1.f, 1.f, 1.f), col(1.f, 1.f, 0.3f), col(1.f, 0.15f, 0.1f) };
    float totalW = p_max.x - p_min.x;
    float segW[5] = { 0.22f, 0.16f, 0.16f, 0.16f, 0.30f }; // same as shader t1-t0, t2-t1, ...
    float x = 0.f;
    for (int i = 0; i < 5; i++) {
      float x0 = p_min.x + totalW * x;
      x += segW[i];
      float x1 = p_min.x + totalW * x;
      dl->AddRectFilledMultiColor(ImVec2(x0, p_min.y), ImVec2(x1, p_max.y), stops[i], stops[i + 1], stops[i + 1], stops[i]);
    }
  }
}

// Canonical source: shaders/trigram_vertex_position_and_density.glsl
static const char* trigramVS = R"(#version 150
in vec3 pos;
in float filePos;
in float density;
uniform mat4 mvp;
uniform float pointSize;
out float vFilePos;
out float vDensity;
void main() {
  gl_Position = mvp * vec4(pos, 1.0);
  gl_PointSize = pointSize;
  vFilePos = filePos;
  vDensity = density;
})";

// Gradient template: see shaders/trigram_fragment_gradient_template.glsl
// colorMode 0 = single, 1 = Fyre (violet->..->red, no green). Brightness scales whole gradient toward black.
static const char* trigramFS = R"(#version 150
in float vFilePos;
in float vDensity;
uniform int invertColors;
uniform int colorMode;
uniform float c_brightness;
out vec4 oColor;
vec3 getGradientColor(float t) {
  if (colorMode == 0) return vec3(0.6, 0.6, 0.6);
  if (colorMode == 1) {
    vec3 violet = vec3(0.58, 0.0, 0.83);
    vec3 blue   = vec3(0.2, 0.5, 1.0);
    vec3 lblue  = vec3(0.4, 0.75, 1.0);
    vec3 white  = vec3(1.0, 1.0, 1.0);
    vec3 yellow = vec3(1.0, 1.0, 0.3);
    vec3 red    = vec3(1.0, 0.15, 0.1);
    // Non-uniform: purple/violet and red get more range so more bins have those colors
    float t0 = 0.0, t1 = 0.22, t2 = 0.38, t3 = 0.54, t4 = 0.70, t5 = 1.0;
    if (t < t1) return mix(violet, blue, (t - t0) / (t1 - t0));
    if (t < t2) return mix(blue, lblue, (t - t1) / (t2 - t1));
    if (t < t3) return mix(lblue, white, (t - t2) / (t3 - t2));
    if (t < t4) return mix(white, yellow, (t - t3) / (t4 - t3));
    return mix(yellow, red, (t - t4) / (t5 - t4));
  }
  return vec3(0.5, 0.5, 0.5);
}
void main() {
  float t = clamp(vFilePos, 0.0, 1.0);
  if (invertColors != 0) t = 1.0 - t;
  vec3 color = getGradientColor(t);
  float v_factor = 0.2 + 0.8 * vDensity;
  vec3 outColor = color * v_factor * c_brightness;
  oColor = vec4(clamp(outColor, 0.0, 1.0), 1.0);
})";

static const char* compositeVS = R"(#version 150
out vec2 vUV;
const vec2 pos[6] = vec2[](
  vec2(-1.0,-1.0), vec2(1.0,-1.0), vec2(1.0,1.0),
  vec2(-1.0,-1.0), vec2(1.0,1.0), vec2(-1.0,1.0));
const vec2 uv[6] = vec2[](
  vec2(0.0,0.0), vec2(1.0,0.0), vec2(1.0,1.0),
  vec2(0.0,0.0), vec2(1.0,1.0), vec2(0.0,1.0));
void main() {
  vUV = uv[gl_VertexID];
  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
})";
static const char* compositeFS = R"(#version 150
in vec2 vUV;
uniform sampler2D uTex;
uniform float uBrightness;
out vec4 oColor;
void main() {
  vec4 c = texture(uTex, vUV);
  oColor = vec4(c.rgb * uBrightness, c.a);
})";

static GLuint g_compositeProgram = 0;
static GLuint g_compositeVAO = 0;

static void trigramCompositePass(int renderW, int renderH, float brightness) {
  if (!g_compositeProgram || !g_compositeVAO || !g.trigramCompositeFBO || !g.trigramCompositeTex) return;
  GLint prevFBO, prevViewport[4], prevProgram, prevVAO, prevTex;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
  glGetIntegerv(GL_VIEWPORT, prevViewport);
  glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

  glBindFramebuffer(GL_FRAMEBUFFER, g.trigramCompositeFBO);
  glViewport(0, 0, renderW, renderH);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_BLEND);
  glUseProgram(g_compositeProgram);
  glUniform1i(glGetUniformLocation(g_compositeProgram, "uTex"), 0);
  glUniform1f(glGetUniformLocation(g_compositeProgram, "uBrightness"), brightness);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g.trigramRenderTex);
  glBindVertexArray(g_compositeVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(prevVAO);
  glUseProgram(prevProgram);
  glBindTexture(GL_TEXTURE_2D, prevTex);
  glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
  glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

static void trigramRenderFBO(int renderW, int renderH) {
  if (g.trigramFBOW != renderW || g.trigramFBOH != renderH) {
    if (g.trigramFBO) {
      glDeleteFramebuffers(1, &g.trigramFBO);
      glDeleteTextures(1, &g.trigramRenderTex);
      glDeleteRenderbuffers(1, &g.trigramDepthRBO);
      g.trigramFBO = g.trigramRenderTex = g.trigramDepthRBO = 0;
    }
    if (g.trigramCompositeFBO) {
      glDeleteFramebuffers(1, &g.trigramCompositeFBO);
      glDeleteTextures(1, &g.trigramCompositeTex);
      g.trigramCompositeFBO = g.trigramCompositeTex = 0;
    }
    glGenFramebuffers(1, &g.trigramFBO);
    glGenTextures(1, &g.trigramRenderTex);
    glGenRenderbuffers(1, &g.trigramDepthRBO);

    glBindTexture(GL_TEXTURE_2D, g.trigramRenderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, renderW, renderH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindRenderbuffer(GL_RENDERBUFFER, g.trigramDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, renderW, renderH);

    glBindFramebuffer(GL_FRAMEBUFFER, g.trigramFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.trigramRenderTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g.trigramDepthRBO);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Trigram FBO incomplete: 0x%x\n", (unsigned)status);
      g.trigramFBO = g.trigramRenderTex = g.trigramDepthRBO = 0;
      g.trigramFBOW = g.trigramFBOH = 0;
    } else {
      g.trigramFBOW = renderW;
      g.trigramFBOH = renderH;
      glGenFramebuffers(1, &g.trigramCompositeFBO);
      glGenTextures(1, &g.trigramCompositeTex);
      glBindTexture(GL_TEXTURE_2D, g.trigramCompositeTex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, renderW, renderH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindFramebuffer(GL_FRAMEBUFFER, g.trigramCompositeFBO);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.trigramCompositeTex, 0);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &g.trigramCompositeFBO);
        glDeleteTextures(1, &g.trigramCompositeTex);
        g.trigramCompositeFBO = g.trigramCompositeTex = 0;
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  GLint prevFBO;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
  GLint prevViewport[4];
  glGetIntegerv(GL_VIEWPORT, prevViewport);
  GLint prevProgram;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
  GLint prevVAO;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
  GLint prevTex;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
  GLboolean prevBlend = glIsEnabled(GL_BLEND);
  GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);
  GLint prevBlendSrc, prevBlendDst;
  glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrc);
  glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDst);

  glBindFramebuffer(GL_FRAMEBUFFER, g.trigramFBO);
  glViewport(0, 0, renderW, renderH);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  float aspect = (float)renderW / (float)renderH;
  float fov = 45.0f * 3.14159f / 180.0f;
  float near_p = 0.01f, far_p = 100.0f;
  float f = 1.0f / tanf(fov / 2.0f);

  float proj[16] = {};
  proj[0] = f / aspect;
  proj[5] = f;
  proj[10] = (far_p + near_p) / (near_p - far_p);
  proj[11] = -1.0f;
  proj[14] = 2.0f * far_p * near_p / (near_p - far_p);

  float rx = g.trigramRotX * 3.14159f / 180.0f;
  float ry = g.trigramRotY * 3.14159f / 180.0f;
  float cx = cosf(rx), sx = sinf(rx);
  float cy = cosf(ry), sy = sinf(ry);

  float view[16] = {};
  view[0] = cy;
  view[1] = sx * sy;
  view[2] = -cx * sy;
  view[4] = 0;
  view[5] = cx;
  view[6] = sx;
  view[8] = sy;
  view[9] = -sx * cy;
  view[10] = cx * cy;
  view[14] = -g.trigramZoom;
  view[15] = 1.0f;

  float mvp[16] = {};
  for (int col = 0; col < 4; col++)
    for (int row = 0; row < 4; row++)
      for (int k = 0; k < 4; k++)
        mvp[row + col * 4] += proj[row + k * 4] * view[k + col * 4];

  if (!g.trigramFBO || !g.trigramShader || !g.trigramVAO || g.trigramVertCount <= 0) {
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    return;
  }
  glUseProgram(g.trigramShader);
  glUniformMatrix4fv(glGetUniformLocation(g.trigramShader, "mvp"), 1, GL_FALSE, mvp);
  glUniform1f(glGetUniformLocation(g.trigramShader, "pointSize"), 1.0f);
  glUniform1i(glGetUniformLocation(g.trigramShader, "invertColors"), g.trigramInvertColors ? 1 : 0);
  glUniform1i(glGetUniformLocation(g.trigramShader, "colorMode"), g.trigramColorMode);
  // Option 3: min/max brightness from data. 0 = lowest, 100 = highest (data-derived).
  float minBrightness = 0.0f;
  float maxBrightness = (g.trigramVertCount > 0)
      ? std::min(1.0f, 1000000.0f / (float)g.trigramVertCount)  // avoid blow-out with additive blend
      : 1.0f;
  float u = std::max(0.0f, std::min(100.0f, g.trigramBrightness)) / 100.0f;
  float c_brightness = minBrightness + u * (maxBrightness - minBrightness);
  glUniform1f(glGetUniformLocation(g.trigramShader, "c_brightness"), c_brightness);

  glBindVertexArray(g.trigramVAO);
  glBindBuffer(GL_ARRAY_BUFFER, g.trigramVBO);
  glDrawArrays(GL_POINTS, 0, g.trigramVertCount);
  glBindVertexArray(0);
  glUseProgram(0);

  glBlendFunc(prevBlendSrc, prevBlendDst);
  if (prevBlend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (prevDepth)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  glBindVertexArray(prevVAO);
  glUseProgram(prevProgram);
  glBindTexture(GL_TEXTURE_2D, prevTex);
  glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
  glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

  g.trigramDirty = false;
}

}  // namespace

void initTrigramGL() {
  if (g.trigramShader) {
    glDeleteProgram(g.trigramShader);
    g.trigramShader = 0;
  }
  GLuint vs = compileShader(GL_VERTEX_SHADER, trigramVS);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, trigramFS);
  GLuint p = linkProgram(vs, fs);
  if (p) g.trigramShader = p;

  if (g_compositeProgram) {
    glDeleteProgram(g_compositeProgram);
    g_compositeProgram = 0;
  }
  if (g_compositeVAO) {
    glDeleteVertexArrays(1, &g_compositeVAO);
    g_compositeVAO = 0;
  }
  GLuint cvs = compileShader(GL_VERTEX_SHADER, compositeVS);
  GLuint cfs = compileShader(GL_FRAGMENT_SHADER, compositeFS);
  GLuint cp = linkProgram(cvs, cfs);
  if (cp) {
    g_compositeProgram = cp;
    glGenVertexArrays(1, &g_compositeVAO);
    glBindVertexArray(g_compositeVAO);
    glBindVertexArray(0);
  }
}

void buildTrigramGeometry() {
  if (g.bytes.size() < 3) return;
  // Viz range must be inside focus (first slider). Never render more than what the first slider selected.
  size_t focusEnd = g.focusEnd != 0 ? g.focusEnd : g.bytes.size();
  size_t focusStart = g.focusStart;
  size_t end = g.vizRangeEnd != 0 ? g.vizRangeEnd : focusEnd;
  size_t start = g.vizRangeStart;
  start = std::max(start, focusStart);
  end = std::min(end, focusEnd);
  if (start >= end || end - start < 3) {
    g.trigramVertCount = 0;
    return;
  }
  size_t n = end - start - 2;
  const int BINS = 32;
  const int BINS3 = BINS * BINS * BINS;
  std::vector<int> binCount(BINS3, 0);

  struct TriVert {
    float x, y, z, pos, density;
  };
  std::vector<TriVert> verts(n);

  size_t fileSz = g.bytes.size();
  for (size_t i = 0; i < n; i++) {
    size_t j = start + i;
    float x = (float(g.bytes[j]) + 0.5f) / 128.0f - 1.0f;
    float y = (float(g.bytes[j + 1]) + 0.5f) / 128.0f - 1.0f;
    float z = (float(g.bytes[j + 2]) + 0.5f) / 128.0f - 1.0f;
    int bx = (int)((x + 1.0f) * 0.5f * BINS);
    bx = std::max(0, std::min(BINS - 1, bx));
    int by = (int)((y + 1.0f) * 0.5f * BINS);
    by = std::max(0, std::min(BINS - 1, by));
    int bz = (int)((z + 1.0f) * 0.5f * BINS);
    bz = std::max(0, std::min(BINS - 1, bz));
    int bin = bx + by * BINS + bz * BINS * BINS;
    binCount[bin]++;
    verts[i].x = x;
    verts[i].y = y;
    verts[i].z = z;
    // Color by position in file (0=start of file, 1=end) so colors stay stable when the slider moves.
    verts[i].pos = (fileSz > 1) ? (float(j) / (float)(fileSz - 1)) : 0.0f;
    verts[i].density = 0.0f;
  }

  int maxCount = 1;
  for (int c : binCount)
    if (c > maxCount) maxCount = c;
  g.trigramMaxDensity = (uint64_t)maxCount;
  for (size_t i = 0; i < n; i++) {
    int bx = (int)((verts[i].x + 1.0f) * 0.5f * BINS);
    bx = std::max(0, std::min(BINS - 1, bx));
    int by = (int)((verts[i].y + 1.0f) * 0.5f * BINS);
    by = std::max(0, std::min(BINS - 1, by));
    int bz = (int)((verts[i].z + 1.0f) * 0.5f * BINS);
    bz = std::max(0, std::min(BINS - 1, bz));
    int bin = bx + by * BINS + bz * BINS * BINS;
    verts[i].density = (float)binCount[bin] / (float)maxCount;
  }

  if (!g.trigramVAO) glGenVertexArrays(1, &g.trigramVAO);
  if (!g.trigramVBO) glGenBuffers(1, &g.trigramVBO);
  glBindVertexArray(g.trigramVAO);
  glBindBuffer(GL_ARRAY_BUFFER, g.trigramVBO);
  glBufferData(GL_ARRAY_BUFFER, n * sizeof(TriVert), verts.data(), GL_STATIC_DRAW);
  if (!g.trigramShader) {
    glBindVertexArray(0);
    g.trigramVertCount = 0;
    return;
  }
  GLint posLoc = glGetAttribLocation(g.trigramShader, "pos");
  GLint fpLoc = glGetAttribLocation(g.trigramShader, "filePos");
  GLint densLoc = glGetAttribLocation(g.trigramShader, "density");
  if (posLoc < 0 || fpLoc < 0 || densLoc < 0) {
    glBindVertexArray(0);
    g.trigramVertCount = 0;
    return;
  }
  glEnableVertexAttribArray(posLoc);
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(TriVert), (void*)0);
  glEnableVertexAttribArray(fpLoc);
  glVertexAttribPointer(fpLoc, 1, GL_FLOAT, GL_FALSE, sizeof(TriVert), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(densLoc);
  glVertexAttribPointer(densLoc, 1, GL_FLOAT, GL_FALSE, sizeof(TriVert), (void*)(4 * sizeof(float)));
  glBindVertexArray(0);
  g.trigramVertCount = (int)n;
}

void drawTrigram() {
  if (!ImGui::Begin("Trigram", &g.showTrigram)) {
    ImGui::End();
    return;
  }
  if (!g.fileLoaded) {
    ImGui::TextDisabled("No data");
    ImGui::End();
    return;
  }
  if (!g.trigramShader) initTrigramGL();
  size_t fileSz = g.bytes.size();

  if (g.openTrigramSettingsPopup) {
    g.openTrigramSettingsPopup = false;
    ImGui::OpenPopup("TrigramSettings");
  }
  if (ImGui::BeginPopup("TrigramSettings")) {
    if (ImGui::Checkbox("Auto-rotate", &g.trigramAutoRotate)) g.trigramDirty = true;
    if (ImGui::Checkbox("Invert colors (blue = start, maroon = end)", &g.trigramInvertColors)) g.trigramDirty = true;
    ImGui::Spacing();
    const char* gradientNames[] = { "Single", "Fyre" };
    const float stripW = 60.f;
    const float stripH = ImGui::GetFrameHeight() * 0.6f;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Gradient");
    ImGui::SameLine(Design::LabelWidth);
    ImVec2 stripPos = ImGui::GetCursorScreenPos();
    drawGradientStrip(stripPos, ImVec2(stripPos.x + stripW, stripPos.y + stripH), g.trigramColorMode);
    ImGui::Dummy(ImVec2(stripW, stripH));
    ImGui::SameLine();
    if (ImGui::BeginCombo("##gradient", gradientNames[g.trigramColorMode])) {
      for (int i = 0; i < 2; i++) {
        ImGui::PushID(i);
        bool selected = (g.trigramColorMode == i);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        drawGradientStrip(pos, ImVec2(pos.x + stripW, pos.y + stripH), i);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + stripW + ImGui::GetStyle().ItemSpacing.x, pos.y));
        if (ImGui::Selectable(gradientNames[i], selected)) {
          g.trigramColorMode = i;
          g.trigramDirty = true;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    ImGui::TextDisabled("White from additive blend. New gradients: see shaders/trigram_fragment_gradient_template.glsl");
    ImGui::Spacing();
    const float kBrightnessStep = 0.1f;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Brightness (0 = lowest, 100 = highest)");
    ImGui::SameLine(Design::LabelWidth);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("-")) { g.trigramBrightness = std::max(0.0f, g.trigramBrightness - kBrightnessStep); g.trigramDirty = true; }
    ImGui::PopStyleColor(4);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(52);
    if (ImGui::InputFloat("##brightness", &g.trigramBrightness, 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) {
      g.trigramBrightness = std::max(0.0f, std::min(100.0f, g.trigramBrightness));
      g.trigramDirty = true;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    if (ImGui::Button("+")) { g.trigramBrightness = std::min(100.0f, g.trigramBrightness + kBrightnessStep); g.trigramDirty = true; }
    ImGui::PopStyleColor(4);
    ImGui::TextDisabled("Min/max are derived from current data. Bright spot = many null-byte trigrams at (0,0,0).");
    ImGui::EndPopup();
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  float parentW = ImGui::GetContentRegionAvail().x;
  float stripW = (parentW * 0.075f);  // 7.5% each, 15% total
  drawVizRangeSelector(g.bytes.data(), fileSz, stripW, [] {
    buildTrigramGeometry();
    buildDigramTexture();
    g.trigramDirty = true;
  });

  ImGui::SameLine(0, 0);
  ImGui::BeginChild("VizMain", ImVec2(0, 0), ImGuiChildFlags_None);
  ImVec2 avail = ImGui::GetContentRegionAvail();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float drawW = avail.x, drawH = avail.y;
  if (drawW < 32 || drawH < 32) {
    ImGui::Dummy(avail);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    return;
  }

  ImGui::InvisibleButton("##trigram_area", avail);

  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    ImVec2 delta = ImGui::GetIO().MouseDelta;
    g.trigramRotY += delta.x * 0.5f;
    g.trigramRotX += delta.y * 0.5f;
    g.trigramDirty = true;
    g.trigramUserDragging = true;
  } else {
    g.trigramUserDragging = false;
  }
  if (ImGui::IsItemHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
      g.trigramZoom -= wheel * 0.2f;
      g.trigramZoom = std::max(0.35f, std::min(8.0f, g.trigramZoom));
      g.trigramDirty = true;
    }
  }

  if (g.trigramAutoRotate && !g.trigramUserDragging) {
    float dt = ImGui::GetIO().DeltaTime;
    g.trigramRotY += 15.0f * dt;
    g.trigramRotX += 3.0f * dt;
    g.trigramDirty = true;
  }

  float dpiScale = ImGui::GetIO().DisplayFramebufferScale.x;
  int renderW = std::max(64, std::min((int)(drawW * dpiScale), 4096));
  int renderH = std::max(64, std::min((int)(drawH * dpiScale), 4096));

  if (g.trigramFBOW != renderW || g.trigramFBOH != renderH) g.trigramDirty = true;

  if (g.trigramDirty) {
    trigramRenderFBO(renderW, renderH);
  }

  if (g.trigramRenderTex) {
    ImVec2 p0 = origin;
    ImVec2 p1 = ImVec2(origin.x + drawW, origin.y + drawH);
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)g.trigramRenderTex, p0, p1,
                                        ImVec2(0, 1), ImVec2(1, 0));
  }

  ImGui::EndChild();

  // Settings button floating over the canvas (top-left so it's not hidden by dock/tabs)
  float btnW = ImGui::CalcTextSize("Settings").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  float btnH = ImGui::GetFrameHeight();
  const float pad = 8.0f;
  float btnX = origin.x + pad;
  float btnY = origin.y + pad;
  ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.28f, 0.32f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.38f, 0.42f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.36f, 0.95f));
  if (ImGui::Button("Settings", ImVec2(btnW, btnH))) {
    ImGui::OpenPopup("TrigramSettings");
  }
  ImGui::PopStyleColor(3);

  ImGui::PopStyleVar();
  ImGui::End();
}
